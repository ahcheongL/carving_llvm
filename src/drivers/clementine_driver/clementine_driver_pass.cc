#include "driver_pass.hpp"
#include "llvm/Demangle/Demangle.h"

namespace {

FunctionCallee sel_file;
FunctionCallee init_driver;
FunctionCallee fetch_file;
FunctionCallee cov_fini;

class clementine_pass : public ModulePass {
 public:
  static char ID;
  clementine_pass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    DEBUG0("Running binary fuzz clementine_pass\n");

    for (auto &F : M) {
      if (F.isIntrinsic() || !F.size()) {
        continue;
      }
      func_list.push_back(&F);
    }

    initialize_pass_contexts(M);

    read_probe_list("driver_probe_names.txt");
    read_probe_list("fuzz_driver_probe_names.txt");
    get_llvm_types();

    get_driver_func_callees();

    get_class_type_info();

    find_global_var_uses();

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
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "instrumenting to make replay driver";
  }

 private:
  bool instrument_module();

  std::string target_name;
  std::vector<Function *> func_list;
  std::map<std::string, std::map<std::string, std::set<std::string>>>
      file_bb_map;

  void instrument_main_func(Function *main_func);
  void instrument_load_class_func(Function *);
  bool is_load_class_func(Function *);
};

}  // namespace

char clementine_pass::ID = 0;

bool clementine_pass::instrument_module() {
  gen_class_replay();

  Function *main_func = NULL;

  sel_file = Mod->getOrInsertFunction("__select_replay_file", Int8PtrTy,
                                      Int8PtrTy, Int32Ty);
  init_driver = Mod->getOrInsertFunction("__driver_initialize", VoidTy);

  fetch_file =
      Mod->getOrInsertFunction("__fetch_file", Int8PtrTy, Int8PtrTy, Int32Ty);

  FunctionCallee record_bb = Mod->getOrInsertFunction(
      "__record_bb_cov", VoidTy, Int8PtrTy, Int8PtrTy, Int8PtrTy);

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

    if (file_bb_map.find(filename) == file_bb_map.end()) {
      file_bb_map.insert({filename, {}});
    }

    if (file_bb_map[filename].find(func_name) == file_bb_map[filename].end()) {
      file_bb_map[filename].insert({func_name, {}});
    }

    Constant *filename_const = gen_new_string_constant(filename, IRB);
    Constant *func_name_const = gen_new_string_constant(func_name, IRB);

    std::set<std::string> &cur_bb_set = file_bb_map[filename][func_name];

    for (auto &BB : F) {
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
    for (auto &BB : F) {
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

void clementine_pass::instrument_main_func(Function *main_func) {
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

bool clementine_pass::is_load_class_func(Function *func) {
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

  PointerType *ret_ptr_ty = dyn_cast<PointerType>(ret_type);

  Type *ret_pointee_ty = ret_type->getPointerElementType();

  if (!ret_pointee_ty->isStructTy()) {
    return false;
  }

  return true;
}

void clementine_pass::instrument_load_class_func(Function *func) {
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

static RegisterPass<clementine_pass> X("driver", "Driver pass", false, false);

static void registerPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  auto p = new clementine_pass();
  PM.add(p);
}

static RegisterStandardPasses RegisterPassOpt(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerPass);

static RegisterStandardPasses RegisterPassO0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);