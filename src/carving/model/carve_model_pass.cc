
#include "carving/carve_model_pass.hpp"

std::set<std::string> custom_carvers;

char CarverMPass::ID = 0;

CarverMPass::CarverMPass() : llvm::ModulePass(ID), func_id(0) {}

bool CarverMPass::runOnModule(llvm::Module &M) {
  llvm::outs() << "Running CarverMPass\n";

  initialize_pass_contexts(M);

  VoidTy = llvm::Type::getVoidTy(*Context);
  Int1Ty = llvm::Type::getInt1Ty(*Context);
  Int8Ty = llvm::Type::getInt8Ty(*Context);
  Int16Ty = llvm::Type::getInt16Ty(*Context);
  Int32Ty = llvm::Type::getInt32Ty(*Context);
  Int64Ty = llvm::Type::getInt64Ty(*Context);
  Int128Ty = llvm::Type::getInt128Ty(*Context);
  FloatTy = llvm::Type::getFloatTy(*Context);
  DoubleTy = llvm::Type::getDoubleTy(*Context);
  Int8PtrTy = llvm::PointerType::get(Int8Ty, 0);
  Int16PtrTy = llvm::PointerType::get(Int16Ty, 0);
  Int32PtrTy = llvm::PointerType::get(Int32Ty, 0);
  Int64PtrTy = llvm::PointerType::get(Int64Ty, 0);
  Int128PtrTy = llvm::PointerType::get(Int128Ty, 0);
  Int8PtrPtrTy = llvm::PointerType::get(Int8PtrTy, 0);
  Int8PtrPtrPtrTy = llvm::PointerType::get(Int8PtrPtrTy, 0);
  FloatPtrTy = llvm::PointerType::get(FloatTy, 0);
  DoublePtrTy = llvm::PointerType::get(DoubleTy, 0);

  mem_allocated_probe = Mod->getOrInsertFunction(
      "__mem_allocated_probe", VoidTy, Int8PtrTy, Int32Ty, Int8PtrTy);
  remove_probe = Mod->getOrInsertFunction("__remove_mem_allocated_probe",
                                          VoidTy, Int8PtrTy);

  record_func_ptr = Mod->getOrInsertFunction("__record_func_ptr", VoidTy,
                                             Int8PtrTy, Int8PtrTy);

  argv_modifier = Mod->getOrInsertFunction("__carver_argv_modifier", VoidTy,
                                           Int32PtrTy, Int8PtrPtrPtrTy);
  __carv_fini = Mod->getOrInsertFunction("__carv_FINI", VoidTy);
  carv_char_func = Mod->getOrInsertFunction("Carv_char", VoidTy, Int8Ty);
  carv_short_func = Mod->getOrInsertFunction("Carv_short", VoidTy, Int16Ty);
  carv_int_func = Mod->getOrInsertFunction("Carv_int", VoidTy, Int32Ty);
  carv_long_func = Mod->getOrInsertFunction("Carv_longtype", VoidTy, Int64Ty);
  carv_longlong_func =
      Mod->getOrInsertFunction("Carv_longlong", VoidTy, Int128Ty);
  carv_float_func = Mod->getOrInsertFunction("Carv_float", VoidTy, FloatTy);
  carv_double_func = Mod->getOrInsertFunction("Carv_double", VoidTy, DoubleTy);
  carv_ptr_func = Mod->getOrInsertFunction("Carv_pointer", Int32Ty, Int8PtrTy,
                                           Int8PtrTy, Int32Ty, Int32Ty);

  carv_func_ptr =
      Mod->getOrInsertFunction("__Carv_func_ptr_name", VoidTy, Int8PtrTy);

  keep_class_info = Mod->getOrInsertFunction("__keep_class_info", VoidTy,
                                             Int8PtrTy, Int32Ty, Int32Ty);

  // Constructs global variables to global symbol table.
  global_carve_ready = Mod->getOrInsertGlobal("__carv_ready", Int8Ty);
  global_cur_class_idx =
      Mod->getOrInsertGlobal("__carv_cur_class_index", Int32Ty);
  global_cur_class_size =
      Mod->getOrInsertGlobal("__carv_cur_class_size", Int32Ty);

  carv_open = Mod->getOrInsertFunction("__carv_open", VoidTy, Int8PtrTy);
  carv_close = Mod->getOrInsertFunction("__carv_close", VoidTy, Int8PtrTy);

  insert_obj_info = Mod->getOrInsertFunction("__insert_obj_info", VoidTy,
                                             Int8PtrTy, Int8PtrTy);

  instrument_module();

  llvm::outs() << "Verifying module...\n";
  std::string out;
  llvm::raw_string_ostream output(out);
  bool has_error = verifyModule(M, &output);

  if (has_error > 0) {
    llvm::outs() << "IR errors : \n";
    llvm::outs() << out;
    return false;
  }

  llvm::outs() << "Verifying done without errors\n";

  return true;
}

#if LLVM_VERSION_MAJOR >= 4
llvm::StringRef
#else
const char *
#endif
CarverMPass::getPassName() const {
  return "carving model instrumentation";
}

bool CarverMPass::instrument_module() {
  get_class_type_info();

  get_instrument_func_set();

  find_global_var_uses();

  gen_class_carver_m();

  DEBUG0("Iterating llvm::Functions...\n");

  carv_file = Mod->getOrInsertFunction("__carv_file", VoidTy, Int8PtrTy);

  for (llvm::Function &F : Mod->functions()) {
    if (is_inst_forbid_func(&F)) {
      continue;
    }

    instrument_func(&F);
  }

  check_and_dump_module();

  delete IRB;
  return true;
}

// Analyze existing llvm::Functions and write on func_types.txt and
// target_funcs.txt.
void CarverMPass::get_instrument_func_set() {
  // Control file containing the llvm::Functions to carve
  std::ifstream targets("targets.txt");

  // std::ofstream outfile("func_types.txt");
  // std::ofstream outfile2("target_funcs.txt");

  for (auto &F : Mod->functions()) {
    if (F.isIntrinsic() || !F.size()) {
      continue;
    }

    const std::string func_name = F.getName().str();
    const std::string demangled_name = llvm::demangle(func_name);

    if (func_name.find("_GLOBAL__sub_I_") != std::string::npos) {
      continue;
    }
    if (func_name.find("__cxx_global_var_init") != std::string::npos) {
      continue;
    }
    if (func_name.find("llvm_gcov") != std::string::npos) {
      continue;
    }

    if (func_name == "__clang_call_terminate") {
      continue;
    }

    llvm::DISubprogram *dbgF = F.getSubprogram();
    if (dbgF != NULL) {
      std::string filename = dbgF->getFilename().str();
      if (filename.find("gcc/") != std::string::npos) {
        continue;
      }
    }

    llvm::ItaniumPartialDemangler Demangler;
    Demangler.partialDemangle(func_name.c_str());
    if (Demangler.isCtorOrDtor()) {
      continue;
    }

    // TODO
    if (F.isVarArg()) {
      continue;
    }

    // DEBUG0("Target llvm::Function : " << F.getName().str() << '\n');
    // if (func_name.find("DefaultChannelTest") == std::string::npos) {
    // continue; } if (func_name.find("TestBody") == std::string::npos) {
    // continue; }

    instrument_func_set.insert(func_name);
    // outfile << func_name;
    // std::string tmp;
    // llvm::raw_string_ostream output(tmp);
    // F.getType()->print(output);
    // outfile << " " << output.str() << "\n";
    // outfile2 << func_name << "\n";
  }

  DEBUG0("# of instrument llvm::Functions : " << instrument_func_set.size()
                                              << "\n");

  instrument_func_set.insert("main");

  // outfile.close();
  // outfile2.close();
}

void CarverMPass::instrument_func(llvm::Function *func) {
  std::string func_name = func->getName().str();

  std::vector<llvm::Instruction *> cast_instrs;
  std::vector<llvm::CallInst *> call_instrs;
  std::vector<llvm::Instruction *> ret_instrs;
  std::vector<llvm::InvokeInst *> invoke_instrs;

  for (llvm::BasicBlock &BB : func->getBasicBlockList()) {
    for (llvm::Instruction &IN : BB) {
      if (llvm::isa<CastInst>(&IN)) {
        cast_instrs.push_back(&IN);
      } else if (llvm::isa<llvm::CallInst>(&IN)) {
        call_instrs.push_back(llvm::dyn_cast<llvm::CallInst>(&IN));
      } else if (llvm::isa<llvm::ReturnInst>(&IN)) {
        ret_instrs.push_back(&IN);
      } else if (llvm::isa<llvm::InvokeInst>(&IN)) {
        invoke_instrs.push_back(llvm::dyn_cast<llvm::InvokeInst>(&IN));
      }
    }
  }

  // Just insert memory tracking probes
  llvm::BasicBlock &entry_block = func->getEntryBlock();
  insert_alloca_probe(entry_block);

  if (instrument_func_set.find(func_name) == instrument_func_set.end()) {
    // Perform only memory tracking
    for (llvm::CallInst *call_instr : call_instrs) {
      llvm::Function *callee = call_instr->getCalledFunction();
      if ((callee == NULL) || (callee->isDebugInfoForProfiling())) {
        continue;
      }
      std::string callee_name = callee->getName().str();
      if (callee_name == "__cxa_allocate_exception") {
        continue;
      }

      if (callee_name == "__cxa_throw") {
        // exception handling
        IRB->SetInsertPoint(call_instr);

        insert_dealloc_probes();

        // Insert fini
        if (func_name == "main") {
          IRB->CreateCall(__carv_fini, {});
        }
      }

      IRB->SetInsertPoint(call_instr->getNextNonDebugInstruction());
      insert_mem_func_call_probe(call_instr, callee_name);
    }

    for (auto ret_instr : ret_instrs) {
      IRB->SetInsertPoint(ret_instr);
      insert_dealloc_probes();
    }

    tracking_allocas.clear();
    return;
  }

  std::string demangled_func_name = llvm::demangle(func_name);

  DEBUG0("Inserting probe in " << demangled_func_name << '\n');

  IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());
  llvm::Constant *func_id_const = llvm::ConstantInt::get(Int32Ty, func_id++);

  // Main argc argv handling
  if (demangled_func_name == "main") {
    instrument_main(func);
    tracking_allocas.clear();
    return;
  } else if (func->isVarArg()) {
    // TODO
  } else {
    // Insert input carving probes
    int param_idx = 0;

    insert_check_carve_ready();

    llvm::Constant *func_name_const =
        gen_new_string_constant(demangled_func_name, IRB);

    IRB->CreateCall(carv_open, {func_name_const});

    unsigned int argidx = 0;
    for (auto &arg_iter : func->args()) {
      llvm::Value *func_arg = &arg_iter;
      const string arg_name = "Arg" + std::to_string(argidx++);
      const string type_name = get_type_str(func_arg->getType());

      llvm::Constant *arg_name_const = gen_new_string_constant(arg_name, IRB);
      llvm::Constant *type_name_const = gen_new_string_constant(type_name, IRB);

      IRB->CreateCall(insert_obj_info, {arg_name_const, type_name_const});
      insert_carve_probe_m(func_arg);
    }

    llvm::BasicBlock *insert_block = IRB->GetInsertBlock();
    insert_global_carve_probe(func);

    IRB->CreateCall(carv_close, {func_name_const});
  }

  DEBUG0("Insert memory tracking for " << demangled_func_name << "\n");

  // Call instr probing
  for (auto call_instr : call_instrs) {
    // insert new/free probe, return llvm::Value probe
    llvm::Function *callee = call_instr->getCalledFunction();
    if (callee == NULL) {
      continue;
    }

    if (callee->isDebugInfoForProfiling()) {
      continue;
    }

    std::string callee_name = callee->getName().str();
    if (callee_name == "__cxa_allocate_exception") {
      continue;
    }

    if (callee_name == "exit") {
      IRB->SetInsertPoint(call_instr);
      IRB->CreateCall(__carv_fini, {});
    } else if (callee_name == "__cxa_throw") {
      // exception handling
      IRB->SetInsertPoint(call_instr);

      insert_dealloc_probes();
      // Insert fini
      if (func_name == "main") {
        IRB->CreateCall(__carv_fini, {});
      }
    } else {
      IRB->SetInsertPoint(call_instr->getNextNonDebugInstruction());
      insert_mem_func_call_probe(call_instr, callee_name);
    }
  }

  for (auto invoke_instr : invoke_instrs) {
    // insert new/free probe, return llvm::Value probe
    llvm::Function *callee = invoke_instr->getCalledFunction();
    if (callee == NULL) {
      continue;
    }

    if (callee->isDebugInfoForProfiling()) {
      continue;
    }

    std::string callee_name = callee->getName().str();
    if (callee_name == "__cxa_allocate_exception") {
      continue;
    }

    if (callee_name == "__cxa_throw") {
      //...?
    } else {
      IRB->SetInsertPoint(
          invoke_instr->getNormalDest()->getFirstNonPHIOrDbgOrLifetime());
      insert_mem_func_call_probe(invoke_instr, callee_name);
    }
  }

  // Probing at return
  for (auto ret_instr : ret_instrs) {
    IRB->SetInsertPoint(ret_instr);
    insert_dealloc_probes();
  }

  tracking_allocas.clear();

  DEBUG0("done in " << demangled_func_name << "\n");

  return;
}

void CarverMPass::insert_carve_probe_m(llvm::Value *val) {
  llvm::Type *val_type = val->getType();

  if (val_type == Int1Ty) {
    llvm::Value *cast_val = IRB->CreateZExt(val, Int8Ty);
    IRB->CreateCall(carv_char_func, {cast_val});
  } else if (val_type == Int8Ty) {
    IRB->CreateCall(carv_char_func, {val});
  } else if (val_type == Int16Ty) {
    IRB->CreateCall(carv_short_func, {val});
  } else if (val_type == Int32Ty) {
    IRB->CreateCall(carv_int_func, {val});
  } else if (val_type == Int64Ty) {
    IRB->CreateCall(carv_long_func, {val});
  } else if (val_type == Int128Ty) {
    IRB->CreateCall(carv_longlong_func, {val});
  } else if (val_type->isIntegerTy()) {
    llvm::Value *cast_val = IRB->CreateZExt(val, Int128Ty);
    IRB->CreateCall(carv_longlong_func, {cast_val});
  } else if (val_type == FloatTy) {
    IRB->CreateCall(carv_float_func, {val});
  } else if (val_type == DoubleTy) {
    IRB->CreateCall(carv_double_func, {val});
  } else if (val_type->isX86_FP80Ty()) {
    llvm::Value *cast_val = IRB->CreateFPCast(val, DoubleTy);
    IRB->CreateCall(carv_double_func, {cast_val});
  } else if (val_type->isStructTy()) {
    // Sould be very simple tiny struct...
    llvm::StructType *struct_type = llvm::dyn_cast<llvm::StructType>(val_type);
    const StructLayout *SL = DL->getStructLayout(struct_type);

    auto memberoffsets = SL->getMemberOffsets();
    int idx = 0;
    for (auto _iter : memberoffsets) {
      llvm::Value *extracted_val = IRB->CreateExtractValue(val, idx);
      insert_carve_probe_m(extracted_val);
      idx++;
    }

  } else if (is_func_ptr_type(val_type)) {
    llvm::Value *ptrval =
        IRB->CreateCast(llvm::Instruction::CastOps::BitCast, val, Int8PtrTy);
    IRB->CreateCall(carv_func_ptr, {ptrval});
  } else if (val_type->isFunctionTy() || val_type->isArrayTy()) {
    // Is it possible to reach here?
  } else if (val_type->isPointerTy()) {
    llvm::PointerType *ptrtype = llvm::dyn_cast<llvm::PointerType>(val_type);
    // llvm::Type that we don't know.
    if (ptrtype->isOpaque() || ptrtype->isOpaquePointerTy()) {
      return;
    }
    llvm::Type *pointee_type = ptrtype->getPointerElementType();

    if (llvm::isa<llvm::StructType>(pointee_type)) {
      llvm::StructType *tmptype =
          llvm::dyn_cast<llvm::StructType>(pointee_type);
      if (tmptype->isOpaque()) {
        return;
      }
    }

    unsigned int pointee_size = DL->getTypeAllocSize(pointee_type);
    if (pointee_size == 0) {
      return;
    }

    llvm::Value *ptrval = val;
    if (val_type != Int8PtrTy) {
      ptrval =
          IRB->CreateCast(llvm::Instruction::CastOps::BitCast, val, Int8PtrTy);
    }

    bool is_class_type = false;
    llvm::Value *default_class_idx = llvm::ConstantInt::get(Int32Ty, -1);
    if (pointee_type->isStructTy()) {
      llvm::StructType *struct_type =
          llvm::dyn_cast<llvm::StructType>(pointee_type);
      auto search = class_name_map.find(struct_type);
      if (search != class_name_map.end()) {
        is_class_type = true;
        default_class_idx =
            llvm::ConstantInt::get(Int32Ty, search->second.first);
      }
    } else if (pointee_type == Int8Ty) {
      // check
      is_class_type = true;
      default_class_idx = llvm::ConstantInt::get(Int32Ty, num_class_name_const);
    }

    llvm::Value *pointee_size_val =
        llvm::ConstantInt::get(Int32Ty, pointee_size);

    std::string typestr = get_type_str(pointee_type);
    llvm::Constant *typestr_const = gen_new_string_constant(typestr, IRB);

    // Call Carv_pointer
    llvm::Value *end_size = IRB->CreateCall(
        carv_ptr_func,
        {ptrval, typestr_const, default_class_idx, pointee_size_val});

    llvm::Value *class_idx = NULL;
    if (is_class_type) {
      pointee_size_val = IRB->CreateLoad(Int32Ty, global_cur_class_size);
      class_idx = IRB->CreateLoad(Int32Ty, global_cur_class_idx);
    }

    llvm::Value *pointer_size = IRB->CreateSDiv(end_size, pointee_size_val);

    llvm::Value *cmp_instr1 =
        IRB->CreateICmpEQ(pointer_size, llvm::ConstantInt::get(Int32Ty, 0));

    llvm::Instruction *split_point = &(*IRB->GetInsertPoint());

    // Make loop block

    llvm::BasicBlock *orig_BB = split_point->getParent();
    llvm::BasicBlock *loopblock = orig_BB->splitBasicBlock(split_point);
    llvm::BasicBlock *const loopblock_start = loopblock;

    IRB->SetInsertPoint(loopblock->getFirstNonPHI());
    PHINode *index_phi = IRB->CreatePHI(Int32Ty, 2);
    index_phi->addIncoming(llvm::ConstantInt::get(Int32Ty, 0), orig_BB);

    llvm::Value *elem_ptr =
        IRB->CreateInBoundsGEP(pointee_type, val, index_phi);

    if (is_class_type) {
      llvm::Value *casted_elem_ptr = IRB->CreateCast(
          llvm::Instruction::CastOps::BitCast, elem_ptr, Int8PtrTy);
      IRB->CreateCall(class_carver, {casted_elem_ptr, class_idx});
    } else {
      insert_gep_carve_probe_m(elem_ptr);
      loopblock = IRB->GetInsertBlock();
    }

    llvm::Value *index_update_instr =
        IRB->CreateAdd(index_phi, llvm::ConstantInt::get(Int32Ty, 1));
    index_phi->addIncoming(index_update_instr, loopblock);

    llvm::Value *cmp_instr2 =
        IRB->CreateICmpSLT(index_update_instr, pointer_size);

    llvm::BasicBlock *endblock =
        loopblock->splitBasicBlock(&(*IRB->GetInsertPoint()));

    IRB->SetInsertPoint(orig_BB->getTerminator());

    llvm::Instruction *BB_term =
        IRB->CreateCondBr(cmp_instr1, endblock, loopblock_start);
    BB_term->removeFromParent();

    // remove old terminator
    llvm::Instruction *old_term = orig_BB->getTerminator();
    ReplaceInstWithInst(old_term, BB_term);

    IRB->SetInsertPoint(loopblock->getTerminator());

    llvm::Instruction *loopblock_term =
        IRB->CreateCondBr(cmp_instr2, loopblock_start, endblock);

    loopblock_term->removeFromParent();

    // remove old terminator
    old_term = loopblock->getTerminator();
    ReplaceInstWithInst(old_term, loopblock_term);

    IRB->SetInsertPoint(endblock->getFirstNonPHIOrDbgOrLifetime());

  } else {
    // Unknown llvm::Type
  }

  return;
}

void CarverMPass::instrument_main(llvm::Function *main_func) {
  llvm::BasicBlock &entry_block = main_func->getEntryBlock();
  IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());

  llvm::Value *new_argc = NULL;
  llvm::Value *new_argv = NULL;

  size_t num_main_args = main_func->arg_size();
  assert(num_main_args == 0 || num_main_args == 2);

  std::vector<llvm::CallInst *> call_instrs;
  std::vector<llvm::ReturnInst *> ret_instrs;

  for (auto &BB : main_func->getBasicBlockList()) {
    for (auto &I : BB) {
      if (llvm::isa<llvm::CallInst>(&I)) {
        call_instrs.push_back(llvm::dyn_cast<llvm::CallInst>(&I));
      } else if (llvm::isa<llvm::ReturnInst>(&I)) {
        ret_instrs.push_back(llvm::dyn_cast<llvm::ReturnInst>(&I));
      }
    }
  }

  if (main_func->arg_size() == 0) {
    main_func->setName("__old_main");
    // Make another copy of main that has argc and argv...
    llvm::FunctionType *new_func_type =
        llvm::FunctionType::get(Int32Ty, {Int32Ty, Int8PtrPtrTy}, false);
    llvm::Function *new_func = llvm::Function::Create(
        new_func_type, llvm::Function::ExternalLinkage, "main", Mod);
    llvm::BasicBlock *new_func_entry =
        llvm::BasicBlock::Create(*Context, "entry", new_func);
    IRB->SetInsertPoint(new_func_entry);
    llvm::Instruction *call_old_main = IRB->CreateCall(main_func, {});
    IRB->CreateRet(call_old_main);

    IRB->SetInsertPoint(call_old_main);
    main_func = new_func;

    forbid_func_set.insert(new_func);
  }

  llvm::Value *argc = main_func->getArg(0);
  llvm::Value *argv = main_func->getArg(1);
  AllocaInst *argc_ptr = IRB->CreateAlloca(Int32Ty);
  AllocaInst *argv_ptr = IRB->CreateAlloca(Int8PtrPtrTy);

  new_argc = IRB->CreateLoad(Int32Ty, argc_ptr);
  new_argv = IRB->CreateLoad(Int8PtrPtrTy, argv_ptr);

  argc->replaceAllUsesWith(new_argc);
  argv->replaceAllUsesWith(new_argv);

  IRB->SetInsertPoint((llvm::Instruction *)new_argc);

  IRB->CreateStore(argc, argc_ptr);
  IRB->CreateStore(argv, argv_ptr);

  IRB->CreateCall(argv_modifier, {argc_ptr, argv_ptr});

  llvm::Instruction *new_argv_load_instr =
      llvm::dyn_cast<llvm::Instruction>(new_argv);

  IRB->SetInsertPoint(new_argv_load_instr->getNextNonDebugInstruction());

  auto globals = Mod->global_values();
  // Global variables memory probing
  for (auto global_iter = globals.begin(); global_iter != globals.end();
       global_iter++) {
    if (!llvm::isa<GlobalVariable>(*global_iter)) {
      continue;
    }

    std::string name = global_iter->getName().str();
    std::string demangled = llvm::demangle(name);

    llvm::GlobalValue *global_v1 = &(*global_iter);

    GlobalVariable *global_v = llvm::dyn_cast<GlobalVariable>(global_v1);
    if (global_v->getName().str().find("llvm.") != std::string::npos) {
      continue;
    }

    llvm::Value *casted_ptr = IRB->CreateCast(
        llvm::Instruction::CastOps::BitCast, global_v, Int8PtrTy);
    llvm::Type *gv_type = global_v->getValueType();
    unsigned int size = DL->getTypeAllocSize(gv_type);

    llvm::Constant *type_name_const = llvm::Constant::getNullValue(Int8PtrTy);

    if (gv_type->isStructTy() &&
        llvm::dyn_cast<llvm::StructType>(gv_type)->hasName()) {
      std::string type_name = gv_type->getStructName().str();
      type_name_const = gen_new_string_constant(type_name, IRB);
    }

    llvm::Value *size_const = llvm::ConstantInt::get(Int32Ty, size);
    IRB->CreateCall(mem_allocated_probe,
                    {casted_ptr, size_const, type_name_const});
  }

  // Record func ptr
  for (auto &Func : Mod->functions()) {
    if (Func.isIntrinsic()) {
      continue;
    }

    const std::string func_name = Func.getName().str();
    const std::string demangled_name = llvm::demangle(func_name);
    llvm::Constant *func_name_const =
        gen_new_string_constant(demangled_name, IRB);
    llvm::Value *cast_val =
        IRB->CreateCast(llvm::Instruction::CastOps::BitCast, &Func, Int8PtrTy);
    IRB->CreateCall(record_func_ptr, {cast_val, func_name_const});
  }

  // Record class llvm::Type string llvm::Constants
  for (auto iter : class_name_map) {
    unsigned int class_size = DL->getTypeAllocSize(iter.first);
    IRB->CreateCall(
        keep_class_info,
        {iter.second.second, llvm::ConstantInt::get(Int32Ty, class_size),
         llvm::ConstantInt::get(Int32Ty, iter.second.first)});
  }

  for (auto call_instr : call_instrs) {
    llvm::Function *callee = call_instr->getCalledFunction();
    if (callee == NULL) {
      continue;
    }
    if (callee->isDebugInfoForProfiling()) {
      continue;
    }

    IRB->SetInsertPoint(call_instr->getNextNonDebugInstruction());
    insert_mem_func_call_probe(call_instr, callee->getName().str());
  }

  for (auto ret_instr : ret_instrs) {
    IRB->SetInsertPoint(ret_instr);
    IRB->CreateCall(__carv_fini, {});
  }

  return;
}

void CarverMPass::insert_gep_carve_probe_m(llvm::Value *gep_val) {
  llvm::PointerType *gep_type =
      llvm::dyn_cast<llvm::PointerType>(gep_val->getType());
  llvm::Type *gep_pointee_type = gep_type->getPointerElementType();

  if (gep_pointee_type->isStructTy()) {
    insert_struct_carve_probe_m(gep_val, gep_pointee_type);
  } else if (gep_pointee_type->isArrayTy()) {
    insert_array_carve_probe_m(gep_val);
  } else if (is_func_ptr_type(gep_pointee_type)) {
    llvm::Value *load_ptr = IRB->CreateLoad(gep_pointee_type, gep_val);
    llvm::Value *cast_ptr = IRB->CreateCast(llvm::Instruction::CastOps::BitCast,
                                            load_ptr, Int8PtrTy);
    IRB->CreateCall(carv_func_ptr, {cast_ptr});
  } else {
    llvm::Value *loadval = IRB->CreateLoad(gep_pointee_type, gep_val);
    insert_carve_probe_m(loadval);
  }

  return;
}

void CarverMPass::insert_array_carve_probe_m(llvm::Value *arr_ptr_val) {
  llvm::Type *arr_ptr_type = arr_ptr_val->getType();
  assert(arr_ptr_type->isPointerTy());
  llvm::PointerType *arr_ptr_type_2 =
      llvm::dyn_cast<llvm::PointerType>(arr_ptr_type);

  llvm::Type *arr_type = arr_ptr_type_2->getPointerElementType();
  assert(arr_type->isArrayTy());

  ArrayType *arr_type_2 = llvm::dyn_cast<ArrayType>(arr_type);
  llvm::Type *arr_elem_type = arr_type_2->getElementType();

  unsigned int arr_size = arr_type_2->getNumElements();

  // Make loop block

  llvm::BasicBlock *orig_block = IRB->GetInsertBlock();
  llvm::BasicBlock *loopblock =
      orig_block->splitBasicBlock(&(*IRB->GetInsertPoint()));
  llvm::BasicBlock *const loopblock_start = loopblock;

  IRB->SetInsertPoint(loopblock->getFirstNonPHIOrDbgOrLifetime());
  PHINode *index_phi = IRB->CreatePHI(Int32Ty, 2);
  index_phi->addIncoming(llvm::ConstantInt::get(Int32Ty, 0), orig_block);

  llvm::Value *getelem_instr = IRB->CreateInBoundsGEP(
      arr_type, arr_ptr_val, {llvm::ConstantInt::get(Int32Ty, 0), index_phi});

  insert_gep_carve_probe_m(getelem_instr);
  loopblock = IRB->GetInsertBlock();

  llvm::Value *index_update_instr =
      IRB->CreateAdd(index_phi, llvm::ConstantInt::get(Int32Ty, 1));
  index_phi->addIncoming(index_update_instr, loopblock);

  llvm::Value *cmp_instr2 = IRB->CreateICmpSLT(
      index_update_instr, llvm::ConstantInt::get(Int32Ty, arr_size));

  llvm::Instruction *cur_ip = &(*IRB->GetInsertPoint());

  llvm::BasicBlock *endblock = loopblock->splitBasicBlock(cur_ip);

  IRB->SetInsertPoint(cur_ip);

  llvm::Instruction *BB_term = IRB->CreateBr(loopblock_start);
  BB_term->removeFromParent();

  // remove old terminator
  llvm::Instruction *old_term = orig_block->getTerminator();
  ReplaceInstWithInst(old_term, BB_term);

  IRB->SetInsertPoint(cur_ip);

  llvm::Instruction *loopblock_term =
      IRB->CreateCondBr(cmp_instr2, loopblock_start, endblock);

  loopblock_term->removeFromParent();

  // remove old terminator
  old_term = loopblock->getTerminator();
  ReplaceInstWithInst(old_term, loopblock_term);

  IRB->SetInsertPoint(endblock->getFirstNonPHIOrDbgOrLifetime());

  return;
}

std::set<std::string> struct_carvers_;
void CarverMPass::insert_struct_carve_probe_m(llvm::Value *struct_ptr,
                                              llvm::Type *type) {
  llvm::StructType *struct_type = llvm::dyn_cast<llvm::StructType>(type);

  auto search2 = class_name_map.find(struct_type);
  if (search2 == class_name_map.end()) {
    insert_struct_carve_probe_m_inner(struct_ptr, type);
  } else {
    llvm::Value *casted = IRB->CreateCast(llvm::Instruction::CastOps::BitCast,
                                          struct_ptr, Int8PtrTy);
    IRB->CreateCall(
        class_carver,
        {casted, llvm::ConstantInt::get(Int32Ty, search2->second.first)});
  }

  return;
}

void CarverMPass::insert_struct_carve_probe_m_inner(llvm::Value *struct_ptr,
                                                    llvm::Type *type) {
  IRBuilderBase::InsertPoint cur_ip = IRB->saveIP();
  llvm::StructType *struct_type = llvm::dyn_cast<llvm::StructType>(type);
  const StructLayout *SL = DL->getStructLayout(struct_type);

  std::string struct_name = struct_type->getName().str();
  struct_name = struct_name.substr(struct_name.find('.') + 1);
  if (struct_name.find("::") != std::string::npos) {
    struct_name = struct_name.substr(struct_name.find("::") + 2);
  }

  std::string struct_carver_name = "__Carv_" + struct_name;
  auto search = struct_carvers_.find(struct_carver_name);
  llvm::FunctionCallee struct_carver = Mod->getOrInsertFunction(
      struct_carver_name, VoidTy, struct_ptr->getType());

  if (search == struct_carvers_.end()) {
    struct_carvers_.insert(struct_carver_name);

    // Define struct carver
    llvm::Function *struct_carv_func =
        llvm::dyn_cast<llvm::Function>(struct_carver.getCallee());

    llvm::BasicBlock *entry_BB =
        llvm::BasicBlock::Create(*Context, "entry", struct_carv_func);

    IRB->SetInsertPoint(entry_BB);
    llvm::Instruction *ret_void_instr = IRB->CreateRetVoid();
    IRB->SetInsertPoint(entry_BB->getFirstNonPHIOrDbgOrLifetime());

    llvm::Value *carver_param = struct_carv_func->getArg(0);

    // depth check
    llvm::Constant *depth_check_const =
        Mod->getOrInsertGlobal("__carv_depth", Int8Ty);

    llvm::Value *depth_check_val = IRB->CreateLoad(Int8Ty, depth_check_const);
    llvm::Value *depth_check_cmp = IRB->CreateICmpSGT(
        depth_check_val, llvm::ConstantInt::get(Int8Ty, STRUCT_CARV_DEPTH));
    llvm::BasicBlock *depth_check_BB =
        llvm::BasicBlock::Create(*Context, "depth_check", struct_carv_func);
    llvm::BasicBlock *do_carving_BB =
        llvm::BasicBlock::Create(*Context, "do_carving", struct_carv_func);
    BranchInst *depth_check_br =
        IRB->CreateCondBr(depth_check_cmp, depth_check_BB, do_carving_BB);
    depth_check_br->removeFromParent();
    ReplaceInstWithInst(ret_void_instr, depth_check_br);

    IRB->SetInsertPoint(depth_check_BB);

    IRB->CreateRetVoid();

    IRB->SetInsertPoint(do_carving_BB);

    llvm::BasicBlock *cur_block = do_carving_BB;

    llvm::Value *add_one_depth =
        IRB->CreateAdd(depth_check_val, llvm::ConstantInt::get(Int8Ty, 1));
    IRB->CreateStore(add_one_depth, depth_check_const);

    llvm::Instruction *depth_store_instr2 =
        IRB->CreateStore(depth_check_val, depth_check_const);

    IRB->CreateRetVoid();

    IRB->SetInsertPoint(depth_store_instr2);

    int elem_idx = 0;
    for (auto iter : struct_type->elements()) {
      llvm::Value *gep =
          IRB->CreateStructGEP(struct_type, carver_param, elem_idx);
      insert_gep_carve_probe_m(gep);
      elem_idx++;
    }
  }

  IRB->restoreIP(cur_ip);
  IRB->CreateCall(struct_carver, {struct_ptr});
  return;
}

void CarverMPass::gen_class_carver_m() {
  class_carver =
      Mod->getOrInsertFunction("__class_carver", VoidTy, Int8PtrTy, Int32Ty);
  llvm::Function *class_carver_func =
      llvm::dyn_cast<llvm::Function>(class_carver.getCallee());

  llvm::BasicBlock *entry_BB =
      llvm::BasicBlock::Create(*Context, "entry", class_carver_func);

  llvm::BasicBlock *default_BB =
      llvm::BasicBlock::Create(*Context, "default", class_carver_func);

  // Put return void ad default block
  IRB->SetInsertPoint(default_BB);
  IRB->CreateRetVoid();

  IRB->SetInsertPoint(entry_BB);

  llvm::Value *carving_ptr = class_carver_func->getArg(0);  // *int8
  llvm::Value *class_idx = class_carver_func->getArg(1);    // int32

  SwitchInst *switch_inst =
      IRB->CreateSwitch(class_idx, default_BB, num_class_name_const + 1);

  for (auto class_type : class_name_map) {
    int case_id = class_type.second.first;
    llvm::BasicBlock *case_block = llvm::BasicBlock::Create(
        *Context, std::to_string(case_id), class_carver_func);
    switch_inst->addCase(llvm::ConstantInt::get(Int32Ty, case_id), case_block);
    IRB->SetInsertPoint(case_block);

    llvm::StructType *class_type_ptr = class_type.first;

    llvm::Value *casted_var =
        IRB->CreateCast(llvm::Instruction::CastOps::BitCast, carving_ptr,
                        llvm::PointerType::get(class_type_ptr, 0));

    insert_struct_carve_probe_m_inner(casted_var, class_type_ptr);
    IRB->CreateRetVoid();
  }

  // default is char *
  int case_id = num_class_name_const;
  llvm::BasicBlock *case_block = llvm::BasicBlock::Create(
      *Context, std::to_string(case_id), class_carver_func);
  switch_inst->addCase(llvm::ConstantInt::get(Int32Ty, case_id), case_block);
  IRB->SetInsertPoint(case_block);

  llvm::Value *load_val = IRB->CreateLoad(Int8Ty, carving_ptr);
  IRB->CreateCall(carv_char_func, {load_val});
  IRB->CreateRetVoid();

  switch_inst->setDefaultDest(case_block);

  default_BB->eraseFromParent();

  return;
}

void CarverMPass::insert_alloca_probe(llvm::BasicBlock &entry_block) {
  std::vector<llvm::AllocaInst *> alloc_instrs;
  llvm::AllocaInst *last_alloc_instr = NULL;

  // Collect all alloca instructions in block
  for (llvm::Instruction &IN : entry_block) {
    if (llvm::isa<llvm::AllocaInst>(&IN)) {
      last_alloc_instr = llvm::dyn_cast<AllocaInst>(&IN);
      alloc_instrs.push_back(last_alloc_instr);
    }
  }

  if (last_alloc_instr != NULL) {
    IRB->SetInsertPoint(last_alloc_instr->getNextNonDebugInstruction());
    for (llvm::AllocaInst *alloc_instr : alloc_instrs) {
      // allocated_type is the type pointed by alloc_instr_type.
      llvm::Type *allocated_type = alloc_instr->getAllocatedType();
      llvm::Type *alloc_instr_type = alloc_instr->getType();

      unsigned int size = DL->getTypeAllocSize(allocated_type);

      llvm::Value *casted_ptr = alloc_instr;
      if (alloc_instr_type != Int8PtrTy) {
        casted_ptr = IRB->CreateCast(llvm::Instruction::CastOps::BitCast,
                                     alloc_instr, Int8PtrTy);
      }

      llvm::Value *size_const = llvm::ConstantInt::get(Int32Ty, size);

      llvm::Constant *type_name_const = llvm::Constant::getNullValue(Int8PtrTy);

      if (allocated_type->isStructTy()) {
        std::string typestr = allocated_type->getStructName().str();
        type_name_const = gen_new_string_constant(typestr, IRB);
      }

      IRB->CreateCall(mem_allocated_probe,
                      {casted_ptr, size_const, type_name_const});
      tracking_allocas.push_back(alloc_instr);
    }
  }
}

void CarverMPass::insert_check_carve_ready() {
  llvm::BasicBlock *cur_block = IRB->GetInsertBlock();

  llvm::BasicBlock *new_end_block =
      cur_block->splitBasicBlock(&(*IRB->GetInsertPoint()));

  llvm::BasicBlock *carve_block = llvm::BasicBlock::Create(
      *Context, "carve_block", cur_block->getParent(), new_end_block);

  IRB->SetInsertPoint(cur_block->getTerminator());

  llvm::Instruction *ready_load_instr =
      IRB->CreateLoad(Int8Ty, global_carve_ready);
  llvm::Value *ready_cmp =
      IRB->CreateICmpEQ(ready_load_instr, ConstantInt::get(Int8Ty, 1));

  llvm::Instruction *ready_br =
      IRB->CreateCondBr(ready_cmp, carve_block, new_end_block);
  ready_br->removeFromParent();
  ReplaceInstWithInst(cur_block->getTerminator(), ready_br);

  IRB->SetInsertPoint(carve_block);

  llvm::Instruction *br_instr = IRB->CreateBr(new_end_block);

  // IRB->SetInsertPoint(new_end_block->getFirstNonPHIOrDbgOrLifetime());
  // IRB->CreateStore(ConstantInt::get(Int8Ty, 1), global_carve_ready);

  IRB->SetInsertPoint(br_instr);
}

// Remove alloca (local variable) memory tracking info.
void CarverMPass::insert_dealloc_probes() {
  for (auto alloc_instr : tracking_allocas) {
    Value *casted_ptr =
        IRB->CreateCast(Instruction::CastOps::BitCast, alloc_instr, Int8PtrTy);
    IRB->CreateCall(remove_probe, {casted_ptr});
  }
}

bool CarverMPass::insert_mem_func_call_probe(llvm::Instruction *call_inst,
                                             std::string callee_name) {
  // IRB->SetInsertPoint(call_inst->getNextNonDebugInstruction());

  if (callee_name == "malloc") {
    // Track malloc
    llvm::Constant *type_name_const = get_mem_alloc_type(call_inst);

    llvm::Value *size = call_inst->getOperand(0);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    IRB->CreateCall(mem_allocated_probe, {call_inst, size, type_name_const});
    return true;
  } else if (callee_name == "realloc") {
    // Track realloc
    llvm::Constant *type_name_const = get_mem_alloc_type(call_inst);
    IRB->CreateCall(remove_probe, {call_inst->getOperand(0)});
    llvm::Value *size = call_inst->getOperand(1);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    IRB->CreateCall(mem_allocated_probe, {call_inst, size, type_name_const});
    return true;
  } else if (callee_name == "free") {
    IRB->CreateCall(remove_probe, {call_inst->getOperand(0)});
    return true;
  } else if (callee_name == "llvm.memcpy.p0i8.p0i8.i64") {
    // Get some hint from memory related functions
    //  Value * size = IN.getOperand(2);
    //  if (size->getType() == Int64Ty) {
    //    size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    //  }
    //  std::vector<Value *> args {IN.getOperand(0), size};
    //  IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "llvm.memmove.p0i8.p0i8.i64") {
    // Value * size = IN.getOperand(2);
    // if (size->getType() == Int64Ty) {
    //   size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    // }
    // std::vector<Value *> args {IN.getOperand(0), size};
    // IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strlen") {
    // Value * add_one = IRB->CreateAdd(&IN, ConstantInt::get(Int64Ty, 1));
    // Value * size = IRB->CreateCast(Instruction::CastOps::Trunc, add_one,
    // Int32Ty); std::vector<Value *> args {IN.getOperand(0), size};
    // IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strncpy") {
    // Value * size = IN.getOperand(2);
    // if (size->getType() == Int64Ty) {
    //   size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    // }
    // std::vector<Value *> args {IN.getOperand(0), size};
    // IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strcpy") {
    // std::vector<Value *> strlen_args;
    // strlen_args.push_back(IN.getOperand(0));
    // Value * strlen_result = IRB->CreateCall(strlen_callee, strlen_args);
    // Value * add_one = IRB->CreateAdd(strlen_result,
    // ConstantInt::get(Int64Ty, 1)); std::vector<Value *> args
    // {IN.getOperand(0), add_one}; IRB->CreateCall(mem_allocated_probe,
    // args);
  } else if ((callee_name == "_Znwm") || (callee_name == "_Znam")) {
    // new operator
    Constant *type_name_const = get_mem_alloc_type(call_inst);

    Value *size = call_inst->getOperand(0);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(llvm::Instruction::CastOps::Trunc, size, Int32Ty);
    }
    IRB->CreateCall(mem_allocated_probe, {call_inst, size, type_name_const});
    return true;
  } else if ((callee_name == "_ZdlPv") || (callee_name == "_ZdaPv")) {
    // delete operator
    IRB->CreateCall(remove_probe, {call_inst->getOperand(0)});
    return true;
  }

  return false;
}

Constant *CarverMPass::get_mem_alloc_type(llvm::Instruction *call_inst) {
  if (call_inst == NULL) {
    return llvm::Constant::getNullValue(Int8PtrTy);
  }

  auto cur_insertpoint = IRB->GetInsertPoint();

  llvm::CastInst *cast_instr;
  if ((cast_instr = llvm::dyn_cast<llvm::CastInst>(cur_insertpoint))) {
    llvm::Type *cast_type = cast_instr->getType();
    if (llvm::isa<llvm::PointerType>(cast_type)) {
      llvm::PointerType *cast_ptr_type =
          llvm::dyn_cast<llvm::PointerType>(cast_type);
      llvm::Type *pointee_type = cast_ptr_type->getPointerElementType();
      if (pointee_type->isStructTy()) {
        std::string typestr = pointee_type->getStructName().str();
        llvm::Constant *typename_const = gen_new_string_constant(typestr, IRB);
        return typename_const;
      }
    }
  }

  return Constant::getNullValue(Int8PtrTy);
}

void CarverMPass::insert_global_carve_probe(llvm::Function *F) {
  auto search = global_var_uses.find(F);
  if (search != global_var_uses.end()) {
    for (auto glob_iter : search->second) {
      std::string glob_name = glob_iter->getName().str();

      llvm::Type *const_type = glob_iter->getType();
      assert(const_type->isPointerTy());
      llvm::PointerType *ptr_type =
          llvm::dyn_cast<llvm::PointerType>(const_type);
      llvm::Type *pointee_type = ptr_type->getPointerElementType();

      llvm::Constant *name_const = gen_new_string_constant(glob_name, IRB);
      llvm::Constant *type_const =
          gen_new_string_constant(get_type_str(pointee_type), IRB);

      IRB->CreateCall(insert_obj_info, {name_const, type_const});

      llvm::Value *glob_val = IRB->CreateLoad(pointee_type, glob_iter);
      insert_carve_probe_m(glob_val);
    }
  }

  return;
}

static llvm::RegisterPass<CarverMPass> X("carve", "Carve pass", false, false);

static void registerPass(const llvm::PassManagerBuilder &,
                         llvm::legacy::PassManagerBase &PM) {
  auto p = new CarverMPass();
  PM.add(p);
}

static llvm::RegisterStandardPasses RegisterPassOpt(
    llvm::PassManagerBuilder::EP_ModuleOptimizerEarly, registerPass);

static llvm::RegisterStandardPasses RegisterPassO0(
    llvm::PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);

// static RegisterStandardPasses RegisterPassLTO(
//     PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
//     registerPass);