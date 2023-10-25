#include "drivers/clementine_driver_pass.hpp"

FunctionCallee sel_file;
FunctionCallee init_driver;
FunctionCallee fetch_file;
FunctionCallee cov_fini;
FunctionCallee record_bb;
Constant *cur_target_func_idx;

FunctionCallee default_class_replay;

FunctionCallee replay_default_char;
FunctionCallee replay_default_short;
FunctionCallee replay_default_int;
FunctionCallee replay_default_long;
FunctionCallee replay_default_longlong;
FunctionCallee replay_default_float;
FunctionCallee replay_default_double;
FunctionCallee replay_default_ptr;
FunctionCallee replay_default_func_ptr;

ClementinePass::ClementinePass() : ModulePass(ClementinePass::ID) {}

char ClementinePass::ID = 0;

bool ClementinePass::runOnModule(Module &M) {
  DEBUG0("Running binary fuzz ClementinePass\n");

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) {
      continue;
    }
    func_list.push_back(&F);
  }

  initialize_pass_contexts(M);
  get_llvm_types();

  get_driver_func_callees();

  get_class_type_info();

  find_global_var_uses();

  get_target_func_idx();

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
ClementinePass::getPassName() const {
  return "instrumenting to make replay driver";
}

bool ClementinePass::instrument_module() {
  gen_class_replay();
  gen_default_class_replay();

  Function *main_func = NULL;

  cur_target_func_idx =
      Mod->getOrInsertGlobal("__cur_target_func_idx", Int32Ty);

  sel_file = Mod->getOrInsertFunction("__select_replay_file", Int8PtrTy,
                                      Int8PtrTy, Int32Ty);
  init_driver = Mod->getOrInsertFunction("__driver_initialize", VoidTy);

  fetch_file =
      Mod->getOrInsertFunction("__fetch_file", Int8PtrTy, Int8PtrTy, Int32Ty);

  record_bb = Mod->getOrInsertFunction("__record_bb_cov", VoidTy, Int8PtrTy,
                                       Int8PtrTy, Int8PtrTy);

  cov_fini = Mod->getOrInsertFunction("__cov_fini", VoidTy);

  for (auto &F : Mod->functions()) {
    std::string mangled_func_name = F.getName().str();
    std::string func_name = llvm::demangle(mangled_func_name);

    if (func_name == "main") {
      main_func = &F;
      instrument_main_func(main_func);
      continue;
    }

    if (func_name.find("__load_input_file") != std::string::npos) {
      IRB->SetInsertPoint(F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
      IRB->CreateCall(fetch_file, {F.getArg(0), F.getArg(1)});
      continue;
    }

    if (is_load_class_func(&F)) {
      instrument_load_class_func(&F);
      continue;
    }

    if (is_load_default_func(&F)) {
      instrument_load_default_func(&F);
      continue;
    }

    if (func_name.find("cl_get_target_func_name") != std::string::npos) {
      IRB->SetInsertPoint(F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
      FunctionCallee get_target_func_name = Mod->getOrInsertFunction(
          "__driver_get_target_func_name", VoidTy, Int32Ty);

      Value *cur_target_func_idx_val =
          IRB->CreateLoad(Int32Ty, cur_target_func_idx);

      IRB->CreateCall(get_target_func_name, {cur_target_func_idx_val});
      continue;
    }

    if (func_name.find("cl_select_default_file") != std::string::npos) {
      instrument_select_file_func(&F);
      continue;
    }

    if (func_name.find("clementine_test") != std::string::npos) {
      instrument_cl_test_driver(&F);
      continue;
    }

    if (func_name.find("clementine") != std::string::npos) {
      continue;
    }

    if (func_name.find("cl_fuzz_") != std::string::npos) {
      continue;
    }

    if (func_name.find("__cl_gen_") != std::string::npos) {
      continue;
    }

    if (F.isIntrinsic()) {
      continue;
    }

    if (func_name.find("_GLOBAL__sub_I_") != std::string::npos) {
      continue;
    }

    if (func_name.find("__cxx_global_var_init") != std::string::npos) {
      continue;
    }

    auto subp = F.getSubprogram();
    if (subp == NULL) {
      continue;
    }

    std::string dirname = subp->getDirectory().str();
    std::string filename = subp->getFilename().str();

    if (dirname != "") {
      filename = dirname + "/" + filename;
    }

    // llvm::errs() << "Instrumenting " << func_name << " in " << filename <<
    // "\n";

    if (filename.find("/usr/bin") != std::string::npos) {
      continue;
    }

    // normal functions under test
    instrument_bb_cov(&F, filename, func_name);

    if (target_func_idx_map.find(&F) == target_func_idx_map.end()) {
      continue;
    }

    // make stubs
    instrument_stub(&F);
  }

  for (auto iter : file_bb_map) {
    std::string filename = iter.first;

    std::string cov_filename = filename + ".cov";
    std::ofstream cov_file(cov_filename);

    for (auto iter2 : iter.second) {
      const std::set<std::string> &bb_set = iter2.second;
      cov_file << "F " << iter2.first << " " << false << "\n";
      for (auto &bb_name : bb_set) {
        cov_file << "b " << bb_name << " " << false << "\n";
      }
    }

    cov_file.close();
  }

  if (main_func == NULL) {
    DEBUG0("FATAL : main function not found.\n");
    return false;
  }

  check_and_dump_module();
  delete IRB;
  return true;
}

void ClementinePass::instrument_main_func(Function *main_func) {
  // remove all BBs

  BasicBlock *entry_block = &main_func->getEntryBlock();
  IRB->SetInsertPoint(entry_block->getFirstNonPHIOrDbgOrLifetime());

  // Record func ptr
  FunctionCallee record_func_ptr = Mod->getOrInsertFunction(
      "__record_func_ptr", VoidTy, Int8PtrTy, Int8PtrTy);

  for (Function *func : func_list) {
    std::string func_name = func->getName().str();

    if (func_name.find("__llvm_gcov") != std::string::npos) {
      continue;
    }

    Constant *func_name_const = gen_new_string_constant(func_name, IRB);
    Value *cast_val =
        IRB->CreateCast(Instruction::CastOps::BitCast, func, Int8PtrTy);
    IRB->CreateCall(record_func_ptr, {cast_val, func_name_const});
  }

  // Record class type string constants
  for (auto iter : class_name_map) {
    unsigned int class_size = DL->getTypeAllocSize(iter.first);
    IRB->CreateCall(keep_class_info,
                    {iter.second.second, ConstantInt::get(Int32Ty, class_size),
                     ConstantInt::get(Int32Ty, iter.second.first)});
  }

  Value *new_argc = NULL;
  Value *new_argv = NULL;

  size_t num_main_args = main_func->arg_size();
  assert(num_main_args == 0 || num_main_args == 2);

  Value *argc = main_func->getArg(0);
  Value *argv = main_func->getArg(1);
  AllocaInst *argc_ptr = IRB->CreateAlloca(Int32Ty);
  AllocaInst *argv_ptr = IRB->CreateAlloca(Int8PtrPtrTy);

  new_argc = IRB->CreateLoad(Int32Ty, argc_ptr);
  new_argv = IRB->CreateLoad(Int8PtrPtrTy, argv_ptr);

  argc->replaceAllUsesWith(new_argc);
  argv->replaceAllUsesWith(new_argv);

  IRB->SetInsertPoint((Instruction *)new_argc);

  IRB->CreateStore(argc, argc_ptr);
  IRB->CreateStore(argv, argv_ptr);

  FunctionCallee argv_modifier = Mod->getOrInsertFunction(
      "__driver_input_argv_modifier", VoidTy, Int32PtrTy, Int8PtrPtrPtrTy);

  IRB->CreateCall(argv_modifier, {argc_ptr, argv_ptr});

  FunctionCallee record_func_idx =
      Mod->getOrInsertFunction("__record_func_idx", VoidTy, Int32Ty, Int8PtrTy);

  // Record target func idx
  for (auto iter : target_func_idx_map) {
    Constant *idx_const = ConstantInt::get(Int32Ty, iter.second);
    const std::string demangeld_name =
        llvm::demangle(iter.first->getName().str());
    Constant *func_name_const = gen_new_string_constant(demangeld_name, IRB);
    IRB->CreateCall(record_func_idx, {idx_const, func_name_const});
  }

  std::set<ReturnInst *> ret_inst_set;
  for (auto &BB : *main_func) {
    for (auto &I : BB) {
      if (ReturnInst *ret_inst = dyn_cast<ReturnInst>(&I)) {
        ret_inst_set.insert(ret_inst);
      }
    }
  }

  for (auto ret_inst : ret_inst_set) {
    IRB->SetInsertPoint(ret_inst);
    IRB->CreateCall(cov_fini, {});
  }
}

bool ClementinePass::is_load_default_func(Function *func) {
  std::string func_name = func->getName().str();
  func_name = llvm::demangle(func_name);

  if (func_name.find("__load_default") == std::string::npos) {
    return false;
  }

  if (func->arg_size() != 0) {
    return false;
  }

  Type *ret_type = func->getReturnType();

  if (!ret_type->isPointerTy()) {
    return false;
  }

  return true;
}

bool ClementinePass::is_load_class_func(Function *func) {
  std::string func_name = func->getName().str();
  func_name = llvm::demangle(func_name);

  if (func_name.find("__load_") == std::string::npos) {
    return false;
  }

  if (func->arg_size() != 1) {
    return false;
  }

  if (func->getArg(0)->getType() != Int32Ty) {
    return false;
  }

  Type *ret_type = func->getReturnType();

  if (!ret_type->isPointerTy()) {
    return false;
  }
  return true;
}

void ClementinePass::instrument_load_class_func(Function *func) {
  std::string func_name = func->getName().str();
  func_name = llvm::demangle(func_name);

  Type *ret_type = func->getReturnType();
  PointerType *ret_ptr_ty = dyn_cast<PointerType>(ret_type);
  Type *ret_pointee_ty = ret_ptr_ty->getPointerElementType();

  StructType *ret_struct_ty = dyn_cast<StructType>(ret_pointee_ty);

  Value *default_class_idx = 0;
  auto search = class_name_map.find(ret_struct_ty);
  if (search != class_name_map.end()) {
    default_class_idx = ConstantInt::get(Int32Ty, search->second.first);
  } else {
    llvm::errs() << "Class not found\n";
    default_class_idx = ConstantInt::get(Int32Ty, 0);
  }

  unsigned pointee_size = DL->getTypeAllocSize(ret_pointee_ty);

  const std::string ret_type_name = get_type_str(ret_type);
  const std::string struct_name = get_type_str(ret_pointee_ty);

  Constant *class_name_const = gen_new_string_constant(struct_name, IRB);

  // remove all BBs
  std::vector<BasicBlock *> BBs;
  for (auto &BB : func->getBasicBlockList()) {
    BBs.push_back(&BB);
  }

  for (auto BB : BBs) {
    BB->dropAllReferences();
  }

  for (auto BB : BBs) {
    BB->eraseFromParent();
  }

  BasicBlock *new_entry_block =
      BasicBlock::Create(*Context, "new_entry_block", func);
  IRB->SetInsertPoint(new_entry_block);

  llvm::FunctionCallee inner_f = Mod->getOrInsertFunction(
      "__replay_load_" + struct_name + "_inner", ret_type, Int32Ty);

  CallInst *ret_val = IRB->CreateCall(inner_f, {func->getArg(0)});
  ReturnInst *ret_inst = IRB->CreateRet(ret_val);

  // Remove all "strange" metadata
  SmallVector<std::pair<unsigned, MDNode *>> MDs = {};
  ret_val->getAllMetadata(MDs);
  for (auto iter : MDs) {
    ret_val->setMetadata(iter.first, NULL);
  }

  MDs.clear();
  ret_inst->getAllMetadata(MDs);
  for (auto iter : MDs) {
    ret_inst->setMetadata(iter.first, NULL);
  }

  Function *inner_func = dyn_cast<Function>(inner_f.getCallee());

  BasicBlock *entry_BB = BasicBlock::Create(*Context, "entry", inner_func);
  IRB->SetInsertPoint(entry_BB);

  Constant *type_name_const = gen_new_string_constant(ret_type_name, IRB);

  Value *id_inner_arg = inner_func->getArg(0);
  Value *sel_file_name =
      IRB->CreateCall(sel_file, {type_name_const, id_inner_arg});
  Value *is_null =
      IRB->CreateICmpEQ(sel_file_name, ConstantPointerNull::get(Int8PtrTy));
  BasicBlock *if_null_BB = BasicBlock::Create(*Context, "if_null", inner_func);
  BasicBlock *if_not_null_BB =
      BasicBlock::Create(*Context, "if_not_null", inner_func);
  IRB->CreateCondBr(is_null, if_null_BB, if_not_null_BB);

  IRB->SetInsertPoint(if_null_BB);
  IRB->CreateRet(ConstantPointerNull::get(ret_ptr_ty));

  IRB->SetInsertPoint(if_not_null_BB);

  IRB->CreateCall(__inputf_open, {sel_file_name});

  Value *replay_result =
      IRB->CreateCall(replay_ptr_func, {default_class_idx,
                                        ConstantInt::get(Int32Ty, pointee_size),
                                        class_name_const});

  Value *class_idx = IRB->CreateLoad(Int32Ty, global_cur_class_index);

  IRB->CreateCall(class_replay, {replay_result, class_idx});

  IRB->CreateCall(init_driver, {});

  Value *casted_result = IRB->CreateBitCast(replay_result, ret_type);

  IRB->CreateRet(casted_result);
}

void ClementinePass::instrument_load_default_func(Function *func) {
  std::string func_name = func->getName().str();
  func_name = llvm::demangle(func_name);

  Type *ret_type = func->getReturnType();
  PointerType *ret_ptr_ty = dyn_cast<PointerType>(ret_type);
  Type *ret_pointee_ty = ret_ptr_ty->getPointerElementType();

  StructType *ret_struct_ty = dyn_cast<StructType>(ret_pointee_ty);

  Value *default_class_idx = 0;
  auto search = class_name_map.find(ret_struct_ty);
  if (search != class_name_map.end()) {
    default_class_idx = ConstantInt::get(Int32Ty, search->second.first);
  } else {
    llvm::errs() << "Class not found\n";
    default_class_idx = ConstantInt::get(Int32Ty, 0);
  }

  unsigned pointee_size = DL->getTypeAllocSize(ret_pointee_ty);

  const std::string ret_type_name = get_type_str(ret_type);
  const std::string struct_name = get_type_str(ret_pointee_ty);

  Constant *class_name_const = gen_new_string_constant(struct_name, IRB);

  // remove all BBs
  std::vector<BasicBlock *> BBs;
  for (auto &BB : func->getBasicBlockList()) {
    BBs.push_back(&BB);
  }

  for (auto BB : BBs) {
    BB->dropAllReferences();
  }

  for (auto BB : BBs) {
    BB->eraseFromParent();
  }

  BasicBlock *new_entry_block =
      BasicBlock::Create(*Context, "new_entry_block", func);
  IRB->SetInsertPoint(new_entry_block);

  llvm::FunctionCallee inner_f = Mod->getOrInsertFunction(
      "__replay_load_default_" + struct_name + "_inner", ret_type);

  CallInst *ret_val = IRB->CreateCall(inner_f, {});
  ReturnInst *ret_inst = IRB->CreateRet(ret_val);

  // Remove all "strange" metadata
  SmallVector<std::pair<unsigned, MDNode *>> MDs = {};
  ret_val->getAllMetadata(MDs);
  for (auto iter : MDs) {
    ret_val->setMetadata(iter.first, NULL);
  }

  MDs.clear();
  ret_inst->getAllMetadata(MDs);
  for (auto iter : MDs) {
    ret_inst->setMetadata(iter.first, NULL);
  }

  Function *inner_func = dyn_cast<Function>(inner_f.getCallee());

  BasicBlock *entry_BB = BasicBlock::Create(*Context, "entry", inner_func);
  IRB->SetInsertPoint(entry_BB);

  Value *replay_result =
      IRB->CreateCall(replay_ptr_func, {default_class_idx,
                                        ConstantInt::get(Int32Ty, pointee_size),
                                        class_name_const});

  Value *class_idx = IRB->CreateLoad(Int32Ty, global_cur_class_index);

  IRB->CreateCall(class_replay, {replay_result, class_idx});

  IRB->CreateCall(init_driver, {});

  Value *casted_result = IRB->CreateBitCast(replay_result, ret_type);

  IRB->CreateRet(casted_result);
}

void ClementinePass::instrument_select_file_func(Function *func) {
  IRB->SetInsertPoint(func->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());

  FunctionCallee select_file_func = Mod->getOrInsertFunction(
      "__driver_select_default_file", VoidTy, Int32Ty, Int32Ty);
  IRB->CreateCall(select_file_func, {func->getArg(0), func->getArg(1)});
}

void ClementinePass::instrument_cl_test_driver(Function *F) {
  if (target_func_idx_map.find(F) == target_func_idx_map.end()) {
    return;
  }

  int func_idx = target_func_idx_map[F];

  IRB->SetInsertPoint(F->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());

  IRB->CreateStore(ConstantInt::get(Int32Ty, func_idx), cur_target_func_idx);
}

void ClementinePass::get_target_func_idx() {
  for (auto &F : Mod->functions()) {
    const std::string mangled_func_name = F.getName().str();
    std::string func_name = llvm::demangle(mangled_func_name);
    if (func_name.find("clementine_test_") == std::string::npos) {
      continue;
    }

    func_name = func_name.substr(0, func_name.size() - 2);

    int func_idx = stoi(func_name.substr(16));

    BasicBlock *return_block = NULL;

    for (auto &BB : F) {
      if (isa<ReturnInst>(BB.getTerminator())) {
        return_block = &BB;
        break;
      }
    }

    if (return_block == NULL) {
      continue;
    }

    std::string target_func_name = "";

    for (auto &IN : *return_block) {
      if (!isa<CallBase>(IN)) {
        continue;
      }

      CallBase *call_inst = dyn_cast<CallBase>(&IN);
      Function *called_func = call_inst->getCalledFunction();
      if (called_func == NULL) {
        continue;
      }

      target_func_idx_map.insert({called_func, func_idx});
    }
  }
}

void ClementinePass::instrument_bb_cov(Function *F, const std::string &filename,
                                       const std::string &func_name) {
  if (file_bb_map.find(filename) == file_bb_map.end()) {
    file_bb_map.insert({filename, {}});
  }

  if (file_bb_map[filename].find(func_name) == file_bb_map[filename].end()) {
    file_bb_map[filename].insert({func_name, {}});
  }

  Constant *filename_const = gen_new_string_constant(filename, IRB);
  Constant *func_name_const = gen_new_string_constant(func_name, IRB);

  std::set<std::string> &cur_bb_set = file_bb_map[filename][func_name];

  for (auto &BB : F->getBasicBlockList()) {
    Instruction *first_inst = BB.getFirstNonPHIOrDbgOrLifetime();
    if (first_inst == NULL) {
      continue;
    }

    if (isa<LandingPadInst>(first_inst)) {
      continue;
    }

    std::string BB_name = BB.getName().str();
    if (BB_name == "") {
      continue;
    }
    cur_bb_set.insert(BB_name);

    IRB->SetInsertPoint(first_inst);
    Constant *bb_name_const = gen_new_string_constant(BB_name, IRB);
    IRB->CreateCall(record_bb,
                    {filename_const, func_name_const, bb_name_const});
  }

  // Insert cov fini
  for (auto &BB : F->getBasicBlockList()) {
    for (auto &IN : BB) {
      if (!isa<CallInst>(IN)) {
        continue;
      }

      CallInst *call_inst = dyn_cast<CallInst>(&IN);
      Function *called_func = call_inst->getCalledFunction();
      if (called_func == NULL) {
        continue;
      }

      std::string called_func_name = called_func->getName().str();
      if (called_func_name != "exit") {
        continue;
      }
      IRB->SetInsertPoint(call_inst);
      IRB->CreateCall(cov_fini, {});
    }
  }
}

void ClementinePass::instrument_stub(Function *Func) {
  int func_idx = target_func_idx_map[Func];

  IRB->SetInsertPoint(Func->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());

  Value *cur_target_idx_val = IRB->CreateLoad(Int32Ty, cur_target_func_idx);

  Value *cmp_val = IRB->CreateICmpEQ(cur_target_idx_val,
                                     ConstantInt::get(Int32Ty, func_idx));

  BasicBlock *entry_block = &Func->getEntryBlock();

  BasicBlock *else_end_block =
      BasicBlock::Create(*Context, "else_end_bb", Func);

  BasicBlock *new_end_block =
      entry_block->splitBasicBlock(&(*IRB->GetInsertPoint()));

  IRB->SetInsertPoint(entry_block->getTerminator());

  Instruction *check_br =
      IRB->CreateCondBr(cmp_val, new_end_block, else_end_block);
  check_br->removeFromParent();
  ReplaceInstWithInst(entry_block->getTerminator(), check_br);

  IRB->SetInsertPoint(else_end_block);

  Type *ret_type = Func->getReturnType();
  if (ret_type->isVoidTy()) {
    IRB->CreateRetVoid();
  } else {
    IRB->CreateRet(Constant::getNullValue(ret_type));
  }
}

void ClementinePass::gen_default_class_replay() {
  default_class_replay = Mod->getOrInsertFunction("__default_class_replay",
                                                  VoidTy, Int8PtrTy, Int32Ty);
  Function *class_replay_func =
      dyn_cast<Function>(default_class_replay.getCallee());

  BasicBlock *entry_BB =
      BasicBlock::Create(*Context, "entry", class_replay_func);

  BasicBlock *default_BB =
      BasicBlock::Create(*Context, "default", class_replay_func);
  IRB->SetInsertPoint(default_BB);
  IRB->CreateRetVoid();

  IRB->SetInsertPoint(entry_BB);

  Value *replaying_ptr = class_replay_func->getArg(0);
  Value *class_idx = class_replay_func->getArg(1);

  SwitchInst *switch_inst =
      IRB->CreateSwitch(class_idx, default_BB, num_class_name_const + 1);

  for (auto class_type : class_name_map) {
    int case_id = class_type.second.first;
    BasicBlock *case_block = BasicBlock::Create(
        *Context, std::to_string(case_id), class_replay_func);
    switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
    IRB->SetInsertPoint(case_block);

    StructType *class_type_ptr = class_type.first;

    Value *casted_var =
        IRB->CreateCast(Instruction::CastOps::BitCast, replaying_ptr,
                        PointerType::get(class_type_ptr, 0));

    // TODO
    insert_default_struct_replay_probe_inner(casted_var, class_type_ptr);
    IRB->CreateRetVoid();
  }

  // char * type
  int case_id = num_class_name_const;
  BasicBlock *case_block =
      BasicBlock::Create(*Context, std::to_string(case_id), class_replay_func);
  switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
  IRB->SetInsertPoint(case_block);

  // TODO : check
  Value *new_value = IRB->CreateCall(replay_default_char, {});

  IRB->CreateStore(new_value, replaying_ptr);
  IRB->CreateRetVoid();

  switch_inst->setDefaultDest(case_block);
  default_BB->eraseFromParent();
  return;
}

void ClementinePass::insert_default_struct_replay_probe_inner(Value *struct_ptr,
                                                              Type *type) {
  StructType *struct_type = dyn_cast<StructType>(type);

  IRBuilderBase::InsertPoint cur_ip = IRB->saveIP();

  std::string struct_name = struct_type->getName().str();
  struct_name = struct_name.substr(struct_name.find('.') + 1);
  if (struct_name.find("::") != std::string::npos) {
    struct_name = struct_name.substr(struct_name.find("::") + 2);
  }

  std::string struct_replay_name = "__Replay__default_" + struct_name;
  auto search = default_struct_replayes.find(struct_replay_name);
  FunctionCallee struct_replay = Mod->getOrInsertFunction(
      struct_replay_name, VoidTy, struct_ptr->getType());

  if (search == default_struct_replayes.end()) {
    default_struct_replayes.insert(struct_replay_name);

    // Define struct carver
    Function *struct_replay_func =
        dyn_cast<Function>(struct_replay.getCallee());

    BasicBlock *entry_BB =
        BasicBlock::Create(*Context, "entry", struct_replay_func);

    IRB->SetInsertPoint(entry_BB);

    BasicBlock *cur_block = entry_BB;
    Value *replay_param = struct_replay_func->getArg(0);

    unsigned num_fields = struct_type->getNumElements();

    for (unsigned elem_idx = 0; elem_idx < num_fields; elem_idx++) {
      Value *gep = IRB->CreateStructGEP(struct_type, replay_param, elem_idx);
      Type *field_type = struct_type->getElementType(elem_idx);
      insert_default_gep_replay_probe(gep);
    }

    IRB->CreateRetVoid();

    forbid_func_set.insert(struct_replay_func);
  }

  IRB->restoreIP(cur_ip);
  IRB->CreateCall(struct_replay, {struct_ptr});
}

void ClementinePass::insert_default_gep_replay_probe(Value *gep_val) {
  PointerType *gep_type = dyn_cast<PointerType>(gep_val->getType());
  Type *gep_pointee_type = gep_type->getPointerElementType();

  if (gep_pointee_type->isStructTy()) {
    insert_default_struct_replay_probe(gep_val, gep_pointee_type);
  } else if (is_func_ptr_type(gep_pointee_type)) {
    Value *func_ptr_val = IRB->CreateCall(replay_default_func_ptr, {});
    Value *casted_val = IRB->CreateBitCast(func_ptr_val, gep_pointee_type);
    IRB->CreateStore(casted_val, gep_val);
  } else if (gep_pointee_type->isArrayTy()) {
    ArrayType *array_type = dyn_cast<ArrayType>(gep_pointee_type);
    Type *array_elem_type = array_type->getArrayElementType();

    unsigned int array_size = array_type->getNumElements();
    unsigned int elem_size = DL->getTypeAllocSize(array_elem_type);
    int idx = 0;
    for (idx = 0; idx < array_size; idx++) {
      // TODO
      //  Value *ptr_result = insert_default_replay_probe(array_elem_type,
      //  NULL); if (ptr_result != NULL) {
      //    Value *array_gep = IRB->CreateInBoundsGEP(
      //        array_type, gep_val,
      //        {ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, idx)});
      //    IRB->CreateStore(ptr_result, array_gep);
      //  }
    }
  } else {
    // TODO
    //  Value *ptr_result = insert_default_replay_probe(gep_pointee_type, NULL);
    //  if (ptr_result != NULL) {
    //    Value *casted_val = IRB->CreateBitCast(ptr_result, gep_pointee_type);
    //    IRB->CreateStore(casted_val, gep_val);
    //  }
  }

  return;
}

Value *insert_replay_probe_cl(Type *typeptr, Value *ptr) {
  Value *result = NULL;

  if (typeptr == Int1Ty) {
    result = IRB->CreateCall(replay_char_func, {});
    result = IRB->CreateCast(Instruction::CastOps::Trunc, result, Int1Ty);
  } else if (typeptr == Int8Ty) {
    result = IRB->CreateCall(replay_char_func, {});
  } else if (typeptr == Int16Ty) {
    result = IRB->CreateCall(replay_short_func, {});
  } else if (typeptr == Int32Ty) {
    result = IRB->CreateCall(replay_int_func, {});
  } else if (typeptr == Int64Ty) {
    result = IRB->CreateCall(replay_long_func, {});
  } else if (typeptr == Int128Ty) {
    result = IRB->CreateCall(replay_longlong_func, {});
  } else if (typeptr->isIntegerTy()) {
    result = IRB->CreateCall(replay_longlong_func, {});
    result = IRB->CreateCast(Instruction::CastOps::Trunc, result, typeptr);
  } else if (typeptr == FloatTy) {
    result = IRB->CreateCall(replay_float_func, {});
  } else if (typeptr == DoubleTy) {
    result = IRB->CreateCall(replay_double_func, {});
  } else if (typeptr->isStructTy()) {
    StructType *struct_type = dyn_cast<StructType>(typeptr);

    unsigned int num_elem = struct_type->getNumElements();

    unsigned int idx = 0;
    for (idx = 0; idx < num_elem; idx++) {
      Type *field_type = struct_type->getElementType(idx);
      Value *carved_val = insert_replay_probe_cl(field_type, NULL);
      if (carved_val == NULL) {
        return NULL;
      }
      result =
          IRB->CreateInsertValue(UndefValue::get(typeptr), carved_val, idx);
    }
  } else if (is_func_ptr_type(typeptr)) {
    result = IRB->CreateCall(replay_func_ptr, {});
    result = IRB->CreateBitCast(result, typeptr);
  } else if (typeptr->isFunctionTy()) {
    // Is it possible to reach here?
  } else if (typeptr->isArrayTy()) {
    // Is it possible to reach here?
  } else if (typeptr->isPointerTy()) {
    PointerType *ptrtype = dyn_cast<PointerType>(typeptr);

    if (ptrtype->isOpaque() || ptrtype->isOpaquePointerTy()) {
      return NULL;
    }

    Type *pointee_type = ptrtype->getPointerElementType();

    if (isa<StructType>(pointee_type)) {
      StructType *tmptype = dyn_cast<StructType>(pointee_type);
      if (tmptype->isOpaque()) {
        return NULL;
      }
    }

    unsigned pointee_size = DL->getTypeAllocSize(pointee_type);
    if (pointee_size == 0) {
      return NULL;
    }

    bool is_class_type = false;
    Value *default_class_idx = NULL;
    Constant *class_name_const =
        gen_new_string_constant(get_type_str(pointee_type), IRB);

    if (pointee_type->isStructTy()) {
      StructType *struct_type = dyn_cast<StructType>(pointee_type);
      auto search = class_name_map.find(struct_type);
      if (search != class_name_map.end()) {
        is_class_type = true;
        default_class_idx = ConstantInt::get(Int32Ty, search->second.first);
      }
    } else if (pointee_type == Int8Ty) {
      is_class_type = true;
      default_class_idx = ConstantInt::get(Int32Ty, num_class_name_const);
    }

    if (is_class_type) {
      result = IRB->CreateCall(
          replay_ptr_func,
          {default_class_idx, ConstantInt::get(Int32Ty, pointee_size),
           class_name_const});
    } else {
      result = IRB->CreateCall(
          replay_ptr_func,
          {ConstantInt::get(Int32Ty, 0),
           ConstantInt::get(Int32Ty, pointee_size), class_name_const});
    }

    result = IRB->CreatePointerCast(result, typeptr);

    Value *pointee_size_val = ConstantInt::get(Int32Ty, pointee_size);

    Value *class_idx = NULL;
    if (is_class_type) {
      pointee_size_val = IRB->CreateLoad(Int32Ty, global_cur_class_size);
      class_idx = IRB->CreateLoad(Int32Ty, global_cur_class_index);
    }

    Value *ptr_bytesize = IRB->CreateLoad(Int32Ty, global_ptr_alloc_size);
    Value *ptr_size = IRB->CreateSDiv(ptr_bytesize, pointee_size_val);

    BasicBlock *start_block = IRB->GetInsertBlock();

    Function *cur_func = start_block->getParent();

    // Make loop block
    BasicBlock *loopblock = BasicBlock::Create(*Context, "loop", cur_func,
                                               start_block->getNextNode());
    BasicBlock *const loopblock_start = loopblock;

    Value *zero_address = IRB->CreateLoad(Int8PtrTy, global_cur_zero_address);
    Value *casted_zero_address = NULL;
    if (!is_class_type) {
      casted_zero_address =
          IRB->CreateCast(Instruction::CastOps::BitCast, zero_address, typeptr);
    }

    Value *cmp_instr1 =
        IRB->CreateICmpSLE(ptr_size, ConstantInt::get(Int32Ty, 0));

    Instruction *temp_br_instr = IRB->CreateBr(loopblock);

    IRB->SetInsertPoint(loopblock);
    PHINode *index_phi = IRB->CreatePHI(Int32Ty, 2);
    index_phi->addIncoming(ConstantInt::get(Int32Ty, 0), start_block);

    if (is_class_type) {
      // Value * casted_result = IRB->CreateBitCast(zero_address, Int8PtrTy);
      Value *elem_ptr = IRB->CreateCall(
          update_class_ptr, {zero_address, index_phi, pointee_size_val});
      IRB->CreateCall(class_replay, {elem_ptr, class_idx});
    } else {
      Value *getelem_instr =
          IRB->CreateGEP(pointee_type, casted_zero_address, index_phi);
      insert_gep_replay_probe(getelem_instr);
    }

    Value *index_update_instr =
        IRB->CreateAdd(index_phi, ConstantInt::get(Int32Ty, 1));
    index_phi->addIncoming(index_update_instr, IRB->GetInsertBlock());

    Value *cmp_instr2 = IRB->CreateICmpSLT(index_update_instr, ptr_size);

    Instruction *temp_br_instr2 = IRB->CreateBr(loopblock);

    BasicBlock *endblock =
        BasicBlock::Create(*Context, "end", cur_func, loopblock->getNextNode());

    IRB->SetInsertPoint(temp_br_instr);

    Instruction *BB_term =
        IRB->CreateCondBr(cmp_instr1, endblock, loopblock_start);
    BB_term->removeFromParent();
    ReplaceInstWithInst(temp_br_instr, BB_term);

    IRB->SetInsertPoint(temp_br_instr2);

    Instruction *loopblock_term =
        IRB->CreateCondBr(cmp_instr2, loopblock_start, endblock);
    loopblock_term->removeFromParent();
    ReplaceInstWithInst(temp_br_instr2, loopblock_term);

    IRB->SetInsertPoint(endblock);

  } else if (typeptr->isX86_FP80Ty()) {
    result = IRB->CreateCall(replay_double_func, {});
    result = IRB->CreateFPCast(result, typeptr);
  } else {
    DEBUGDUMP(typeptr);
    DEBUG0("Warning : Unknown type\n");
  }
  return result;
}

static RegisterPass<ClementinePass> X("driver", "Driver pass", false, false);

static void registerPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  auto p = new ClementinePass();
  PM.add(p);
}

static RegisterStandardPasses RegisterPassOpt(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerPass);

static RegisterStandardPasses RegisterPassO0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);