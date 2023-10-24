#include "carving/carve_func_ctx_pass.hpp"

char CarverFCPass::ID = 0;

std::set<std::string> custom_carvers = {
    "class_std__basic_ofstream", "class_std__basic_ostream",
    // "class_std__basic_streambuf"
};

bool CarverFCPass::runOnModule(Module &M) {
  DEBUG0("Running CarverFCPass\n");

  initialize_pass_contexts(M);

  get_llvm_types();
  get_carving_func_callees_and_globals(true);

  instrument_module();

  DEBUG0("Verifying module...\n");
  std::string out;
  llvm::raw_string_ostream output(out);
  bool has_error = verifyModule(M, &output);

  if (has_error > 0) {
    DEBUG0("IR errors : \n");
    DEBUG0(out);
    return false;
  }

  DEBUG0("Verifying done without errors\n");

  return true;
}

#if LLVM_VERSION_MAJOR >= 4
StringRef
#else
const char *
#endif
CarverFCPass::getPassName() const {
  return "carving function argument instrumentation";
}

void CarverFCPass::Insert_return_val_probe(Instruction *IN, Function *callee) {
  std::string callee_name;
  if (callee != NULL) {
    if (callee->isDebugInfoForProfiling()) {
      return;
    }
    callee_name = callee->getName().str();
    if (callee->isIntrinsic()) {
      return;
    }
    if (callee->size() == 0) {
      return;
    }

    if (callee_name == "cxa_allocate_exception") {
      return;
    }
    if (callee_name == "cxa_throw") {
      return;
    }
    if (callee_name == "main") {
      return;
    }
    if (callee_name == "__cxx_global_var_init") {
      return;
    }
    if (callee_name.find("_GLOBAL__sub_I_") != std::string::npos) {
      return;
    }
  }

  IRB->SetInsertPoint(IN->getNextNonDebugInstruction());

  Type *ret_type = IN->getType();
  if (ret_type != VoidTy) {
    if (callee == NULL) {
      // function ptr call, need to check if it will be a stub or not.
      CallInst *call_inst = dyn_cast<CallInst>(IN);
      Value *callee_val = call_inst->getCalledOperand();

      if (isa<InlineAsm>(callee_val)) {
        return;
      }

      BasicBlock *cur_block = IRB->GetInsertBlock();
      BasicBlock *new_end_block =
          cur_block->splitBasicBlock(&(*IRB->GetInsertPoint()));

      BasicBlock *check_carve_block = BasicBlock::Create(
          *Context, "check_carve", cur_block->getParent(), new_end_block);
      IRB->SetInsertPoint(check_carve_block);
      IRB->CreateBr(new_end_block);

      IRB->SetInsertPoint(cur_block->getTerminator());

      Value *casted_callee =
          IRB->CreateCast(Instruction::CastOps::BitCast, callee_val, Int8PtrTy);
      Value *is_no_stub_val = IRB->CreateCall(is_no_stub, {casted_callee});
      Value *is_no_stub_bool =
          IRB->CreateICmpEQ(is_no_stub_val, ConstantInt::get(Int8Ty, 1));
      Instruction *to_check_br =
          IRB->CreateCondBr(is_no_stub_bool, new_end_block, check_carve_block);
      to_check_br->removeFromParent();
      ReplaceInstWithInst(cur_block->getTerminator(), to_check_br);

      IRB->SetInsertPoint(check_carve_block->getTerminator());
    }

    insert_check_carve_ready();

    insert_carve_probe(IN, IRB->GetInsertBlock());
  }
}

void CarverFCPass::insert_global_carve_probe(Function *F, BasicBlock *BB) {
  BasicBlock *cur_block = BB;

  auto search = global_var_uses.find(F);
  if (search != global_var_uses.end()) {
    for (auto glob_iter : search->second) {
      std::string glob_name = glob_iter->getName().str();

      Type *const_type = glob_iter->getType();
      assert(const_type->isPointerTy());
      PointerType *ptr_type = dyn_cast<PointerType>(const_type);
      Type *pointee_type = ptr_type->getPointerElementType();
      Constant *glob_name_const = gen_new_string_constant(glob_name, IRB);

      IRB->CreateCall(insert_obj_info,
                      {glob_name_const, gen_new_string_constant(
                                            get_type_str(pointee_type), IRB)});

      Value *glob_val = IRB->CreateLoad(pointee_type, glob_iter);
      cur_block = insert_carve_probe(glob_val, cur_block);

      carved_types_file << "**" << glob_name << " : "
                        << get_type_str(pointee_type) << "\n";
    }
  }

  return;
}

bool CarverFCPass::instrument_module() {
  get_class_type_info();

  get_instrument_func_set();

  find_global_var_uses();

  gen_class_carver();

  carved_types_file.open("carved_types.txt");

  DEBUG0("Iterating functions...\n");

  llvm::FunctionCallee carv_file =
      Mod->getOrInsertFunction("__carv_file", VoidTy, Int8PtrTy);

  for (llvm::Function &F : Mod->functions()) {
    if (is_inst_forbid_func(&F)) {
      continue;
    }

    std::string func_name = F.getName().str();

    std::vector<Instruction *> cast_instrs;
    std::vector<CallInst *> call_instrs;
    std::vector<Instruction *> ret_instrs;
    std::vector<InvokeInst *> invoke_instrs;

    for (llvm::BasicBlock &BB : F) {
      for (llvm::Instruction &IN : BB) {
        if (isa<CastInst>(&IN)) {
          cast_instrs.push_back(&IN);
        } else if (isa<CallInst>(&IN)) {
          call_instrs.push_back(dyn_cast<CallInst>(&IN));
        } else if (isa<ReturnInst>(&IN)) {
          ret_instrs.push_back(&IN);
        } else if (isa<InvokeInst>(&IN)) {
          invoke_instrs.push_back(dyn_cast<InvokeInst>(&IN));
        }
      }
    }

    // Insert memory tracking probes
    BasicBlock &entry_block = F.getEntryBlock();
    Insert_alloca_probe(entry_block);

    // Memory tracking...
    for (llvm::CallInst *call_instr : call_instrs) {
      Function *callee = call_instr->getCalledFunction();
      if ((callee == NULL) || (callee->isDebugInfoForProfiling())) {
        continue;
      }
      const std::string callee_name = callee->getName().str();
      if (callee_name == "__cxa_allocate_exception") {
        continue;
      }

      // exception handling
      if (callee_name == "__cxa_throw") {
        IRB->SetInsertPoint(call_instr);

        insert_dealloc_probes();

        // Insert fini
        if (func_name == "main") {
          IRB->CreateCall(__carv_fini, {});
        }
      }

      IRB->SetInsertPoint(call_instr->getNextNonDebugInstruction());
      Insert_mem_func_call_probe(call_instr, callee_name);
    }

    for (llvm::InvokeInst *invoke_instr : invoke_instrs) {
      Function *callee = invoke_instr->getCalledFunction();
      if ((callee == NULL) || (callee->isDebugInfoForProfiling())) {
        continue;
      }

      const std::string callee_name = callee->getName().str();
      // ofstream tracking
      const std::string demangle_name = demangle(callee_name);

      if (demangle_name ==
          "std::basic_ofstream<char, std::char_traits<char> >::open(char "
          "const*, std::_Ios_Openmode)") {
        if (invoke_instr->getNumArgOperands() < 2) {
          continue;
        }

        Value *arg0 = invoke_instr->getArgOperand(0);
        Value *arg1 = invoke_instr->getArgOperand(1);

        Type *arg0_type = arg0->getType();
        Type *arg1_type = arg1->getType();
        if ((!arg0_type->isPointerTy()) || (arg1_type != Int8PtrTy)) {
          continue;
        }

        IRB->SetInsertPoint(invoke_instr);
        Value *arg0_casted =
            IRB->CreateCast(Instruction::CastOps::BitCast, arg0, Int8PtrTy);
        IRB->CreateCall(record_ofstream, {arg0_casted, arg1});
      }
    }

    for (auto ret_instr : ret_instrs) {
      IRB->SetInsertPoint(ret_instr);
      insert_dealloc_probes();
    }

    tracking_allocas.clear();

    if (instrument_func_set.find(func_name) == instrument_func_set.end()) {
      continue;
    }

    DEBUG0("Inserting probe in " << func_name << '\n');

    const std::string demangled_func_name = llvm::demangle(func_name);
    carved_types_file << "##" << demangled_func_name << '\n';

    IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());
    Constant *func_id_const = ConstantInt::get(Int32Ty, func_id++);
    Instruction *init_probe = IRB->CreateCall(carv_func_call, {func_id_const});

    // Main argc argv handling
    if (demangled_func_name == "main") {
      Insert_carving_main_probe(&entry_block, &F, NULL);
      continue;
    } else if (F.isVarArg()) {
      // TODO, unreachable
    } else {
      // Insert input carving probes
      int parm_idx = 0;

      insert_check_carve_ready();

      BasicBlock *insert_block = IRB->GetInsertBlock();

      std::vector<std::string> input_files = {
          "branches",      "cfg_branches",      "input", "type", "coverage",
          "szd_execution", "szd_execution_temp"};

      for (auto iter : input_files) {
        Constant *input_name_const = gen_new_string_constant(iter, IRB);
        IRB->CreateCall(carv_file, {input_name_const});
      }

      for (auto &arg_iter : F.args()) {
        Value *func_arg = &arg_iter;

        const std::string parm_name = "parm_" + std::to_string(parm_idx);
        const std::string type_name = get_type_str(func_arg->getType());

        carved_types_file << "**" << parm_name << " : " << type_name << "\n";

        Constant *parm_name_const = gen_new_string_constant(parm_name, IRB);
        Constant *type_name_const = gen_new_string_constant(type_name, IRB);

        IRB->CreateCall(insert_obj_info, {parm_name_const, type_name_const});

        insert_block = insert_carve_probe(func_arg, insert_block);
        parm_idx++;
      }

      insert_global_carve_probe(&F, insert_block);
    }

    IRB->CreateCall(update_carved_ptr_idx, {});

    DEBUG0("Insert memory tracking for " << demangled_func_name << "\n");

    // Call instr probing
    for (auto call_instr : call_instrs) {
      // insert new/free probe, return value probe
      Function *callee = call_instr->getCalledFunction();
      Insert_return_val_probe(call_instr, callee);
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

        const std::string func_ret_obj_str = demangled_func_name + "_ret";
        Constant *func_ret_obj_const =
            gen_new_string_constant(func_ret_obj_str, IRB);

        const std::string type_name = get_type_str(call_instr->getType());
        Constant *type_name_const = gen_new_string_constant(type_name, IRB);

        IRB->CreateCall(insert_obj_info, {func_ret_obj_const, type_name_const});

        Constant *func_name_const =
            gen_new_string_constant(demangled_func_name, IRB);

        IRB->CreateCall(carv_func_ret, {func_name_const, func_id_const});

        insert_dealloc_probes();

        // Insert fini
        if (func_name == "main") {
          IRB->CreateCall(__carv_fini, {});
        }
      } else {
        IRB->SetInsertPoint(call_instr->getNextNonDebugInstruction());
        Insert_mem_func_call_probe(call_instr, callee_name);
      }
    }

    for (auto invoke_instr : invoke_instrs) {
      // insert new/free probe, return value probe
      Function *callee = invoke_instr->getCalledFunction();
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
        Insert_mem_func_call_probe(invoke_instr, callee_name);
      }
    }

    // Probing at return
    for (auto ret_instr : ret_instrs) {
      IRB->SetInsertPoint(ret_instr);

      // Write carved result
      Constant *func_name_const =
          gen_new_string_constant(demangled_func_name, IRB);

      IRB->CreateCall(carv_func_ret, {func_name_const, func_id_const});

      insert_dealloc_probes();
    }

    tracking_allocas.clear();

    DEBUG0("done in " << demangled_func_name << "\n");
  }

  carved_types_file.close();

  check_and_dump_module();

  delete IRB;
  return true;
}

// Analyze existing functions and write on func_types.txt and target_funcs.txt.
void CarverFCPass::get_instrument_func_set() {
  // Control file containing the functions to carve
  std::ifstream targets("targets.txt");

  std::ofstream outfile("func_types.txt");
  std::ofstream outfile2("target_funcs.txt");

  // If targets.txt exists
  if (targets.good()) {
    DEBUG0("Reading targets from targets.txt\n");
    std::string line;
    while (std::getline(targets, line)) {
      if (line.length() == 0) {
        continue;
      }
      if (line[0] == '#') {
        continue;
      }
      instrument_func_set.insert(line);
    }

    instrument_func_set.insert("main");

    for (auto &F : Mod->functions()) {
      std::string func_name = F.getName().str();
      if (instrument_func_set.find(func_name) != instrument_func_set.end()) {
        outfile << func_name;
        std::string tmp;
        llvm::raw_string_ostream output(tmp);
        F.getType()->print(output);
        outfile << " " << output.str() << "\n";
        outfile2 << func_name << "\n";
      }
    }
    DEBUG0("# of instrument functions : " << instrument_func_set.size()
                                          << "\n");
    outfile.close();
    outfile2.close();

    return;
  }

  // Otherwise
  for (auto &F : Mod->functions()) {
    if (F.isIntrinsic() || !F.size()) {
      continue;
    }

    std::string func_name = F.getName().str();
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

    // DEBUG0("Target function : " << F.getName().str() << '\n');
    // if (func_name.find("DefaultChannelTest") == std::string::npos) {
    // continue; } if (func_name.find("TestBody") == std::string::npos) {
    // continue; }

    instrument_func_set.insert(func_name);
    outfile << func_name;
    std::string tmp;
    llvm::raw_string_ostream output(tmp);
    F.getType()->print(output);
    outfile << " " << output.str() << "\n";
    outfile2 << func_name << "\n";
  }

  DEBUG0("# of instrument functions : " << instrument_func_set.size() << "\n");

  instrument_func_set.insert("main");

  outfile.close();
  outfile2.close();
}

static RegisterPass<CarverFCPass> X("carve", "Carve pass", false, false);

static void registerPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  auto p = new CarverFCPass();
  PM.add(p);
}

static RegisterStandardPasses RegisterPassOpt(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerPass);

static RegisterStandardPasses RegisterPassO0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);

// static RegisterStandardPasses RegisterPassLTO(
//     PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
//     registerPass);
