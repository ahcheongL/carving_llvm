#include "driver_pass.hpp"

namespace {

class driver_pass : public ModulePass {

 public:
  static char ID;
  driver_pass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {

    DEBUG0("Running binary fuzz driver_pass\n");

    for (auto &F : M) {
      if (F.isIntrinsic() || !F.size()) {
        continue;
      }
      func_list.push_back(&F);
    }

    initialize_pass_contexts(M);

    bool res = get_target_func();
    if (res == false) {
      DEBUG0("get_target_func failed\n");
      DEBUG0("Print out the list of functions\n");

      dump_func_info();
      return true;
    }

    read_probe_list("driver_probe_names.txt");
    read_probe_list("fuzz_driver_probe_names.txt");
    get_llvm_types();

    get_driver_func_callees();

    replay_char_func = Mod->getOrInsertFunction(get_link_name("Replay_char2"), Int8Ty);
    replay_short_func = Mod->getOrInsertFunction(get_link_name("Replay_short2"), Int16Ty);
    replay_int_func = Mod->getOrInsertFunction(get_link_name("Replay_int2"), Int32Ty);
    replay_long_func = Mod->getOrInsertFunction(get_link_name("Replay_long2"), Int64Ty);
    replay_float_func = Mod->getOrInsertFunction(get_link_name("Replay_float2"), FloatTy);
    replay_double_func = Mod->getOrInsertFunction(get_link_name("Replay_double2"), DoubleTy);
    replay_longlong_func = Mod->getOrInsertFunction(get_link_name("Replay_longlong2"), Int64Ty);
    replay_ptr_func = Mod->getOrInsertFunction(get_link_name("Replay_pointer2"), Int8PtrTy, Int32Ty, Int32Ty, Int8PtrTy);
    replay_func_ptr = Mod->getOrInsertFunction(get_link_name("Replay_func_ptr2"), Int8PtrTy);

    get_class_type_info();

    find_global_var_uses();

    instrument_module();

    DEBUG0("Verifying module...\n");
    std::string out;
    llvm::raw_string_ostream  output(out);
    bool has_error =  verifyModule(M, &output);

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
  Function * target_func;
  std::vector<Function *> func_list;

  bool get_target_func();
  void instrument_main_func(Function * main_func);
  void dump_func_info();
};

}  // namespace

char driver_pass::ID = 0;

bool driver_pass::instrument_module() {

  gen_class_replay();

  Function * main_func = NULL;

  for (auto &F : Mod->functions()) {
    if (is_inst_forbid_func(&F)) { continue; }
    std::string func_name = F.getName().str();

    if (func_name == "main") {
      main_func = &F;
      instrument_main_func(main_func);
    }
  }

  for (auto &F : Mod->functions()) {
    if (is_inst_forbid_func(&F)) { continue; }
    if (main_func != &F && target_func != &F) {
      //make_stub(&F);
    }
  }

  if (main_func == target_func) {
    DEBUG0("Unimplemented : main function can not be the target function.\n");
    return false;
  }

  if (main_func == NULL) {
    DEBUG0("FATAL : main function not found.\n");
    return false;
  }

  check_and_dump_module();
  delete IRB;
  return true;
}

void driver_pass::instrument_main_func(Function * main_func) {

  //remove all BBs
  std::vector<BasicBlock *> BBs;
  for (auto &BB: main_func->getBasicBlockList()) {
    BBs.push_back(&BB);
  }

  for (auto BB: BBs) {
    BB->dropAllReferences();
  }

  for (auto BB: BBs) {
    BB->eraseFromParent();
  }

  BasicBlock * new_entry_block = BasicBlock::Create(*Context, "new_entry_block", main_func);

  BasicBlock * cur_block = new_entry_block;

  IRB->SetInsertPoint(cur_block);

  //Record func ptr
  FunctionCallee record_func_ptr_index = Mod->getOrInsertFunction(get_link_name("__record_func_ptr_index"), VoidTy, Int8PtrTy);
  for (Function * func : func_list) {
    Value *cast_val =
        IRB->CreateCast(Instruction::CastOps::BitCast, func, Int8PtrTy);
    IRB->CreateCall(record_func_ptr_index, {cast_val});
  }

  //Record class type string constants
  for (auto iter : class_name_map) {
    unsigned int class_size = DL->getTypeAllocSize(iter.first);
    IRB->CreateCall(keep_class_info, {iter.second.second
      , ConstantInt::get(Int32Ty, class_size)
      , ConstantInt::get(Int32Ty, iter.second.first)});
  }

  Value * argv = main_func->getArg(1);
  Value * argv_1 = IRB->CreateGEP(Int8PtrTy, argv, ConstantInt::get(Int32Ty, 1));
  argv_1 = IRB->CreateLoad(Int8PtrTy, argv_1);

  FunctionCallee input_fb_open = Mod->getOrInsertFunction(get_link_name("__driver_inputfb_open"), VoidTy, Int8PtrTy);

  IRB->CreateCall(input_fb_open, {argv_1});

  std::vector<Value *> target_args;
  int arg_idx = 0;
  for (auto &arg : target_func->args()) {
    Type * arg_type = arg.getType();
    Value * replay_res = insert_replay_probe(arg_type, NULL);
    target_args.push_back(replay_res);
  }

  auto search = global_var_uses.find(target_func);
  if (search != global_var_uses.end()) {
    for (auto glob_iter : search->second) {
      PointerType * val_type = dyn_cast<PointerType>(glob_iter->getType());
      Type * glob_pointee_type = val_type->getPointerElementType();
      
      Value * replay_res = insert_replay_probe(glob_pointee_type, NULL);      
      if (replay_res != NULL) {
        IRB->CreateStore(replay_res, glob_iter);
      }
    }
  }

  CallInst * target_call = IRB->CreateCall(
    target_func->getFunctionType(), target_func, target_args);

  target_func->setSubprogram(0);

  //Return
  IRB->CreateCall(__replay_fini, {});

  IRB->CreateRet(ConstantInt::get(Int32Ty, 0));
}

bool driver_pass::get_target_func() {
  char * target_name = getenv("TARGET_NAME");
  if (target_name == NULL) {
    DEBUG0("TARGET_NAME is not set\n");
    return false;
  }

  target_func = NULL;

  for (auto &F : Mod->functions()) {
    if (F.isIntrinsic() || !F.size()) { continue; }
    std::string func_name = F.getName().str();    
    if (func_name == target_name) {
      target_func = &F;
      break;
    }
  }

  if (target_func == NULL) {
    DEBUG0("Can't find target : " << target_name << "\n");
    return false;
  }

  return true;
}

void driver_pass::dump_func_info() {
  std::error_code EC1;
  llvm::raw_fd_ostream out1("func.list", EC1);

  CallGraph cg = CallGraph(*Mod);
  std::error_code EC2;
  llvm::raw_fd_ostream out2("callgraph.txt", EC2);

  std::set<StringRef> func_names;
  for (Function * func : func_list) {
    StringRef func_name = func->getName();
    if (func_name.contains("llvm.")) {
      continue;
    }

    if (func_name.contains('.')) {
      func_name = func_name.substr(0, func_name.find('.'));
    }
    
    if (func_names.find(func_name) == func_names.end()) {
      out1 << func_name << "\n";
      func_names.insert(func_name);

      const CallGraphNode * node = cg[func];
      std::set<StringRef> called_funcs;
      for (auto & edge : *node) {
        const Function * callee = edge.second->getFunction();
        if (callee == nullptr) {
          continue;
        }
        StringRef callee_name = callee->getName();
        if (callee_name.contains("llvm.")) {
          continue;
        }
        if (callee_name.contains('.')) {
          callee_name = callee_name.substr(0, callee_name.find('.'));
        }
        called_funcs.insert(callee_name);
      }
      
      out2 << func_name << ":" << called_funcs.size() << "\n";
      for (auto & called_func : called_funcs) {
        out2 << called_func << "\n";
      }
    }
  }
  out1.close();
  out2.close();
}

static RegisterPass<driver_pass> X("fuzz_driver_pass", "Driver pass", false , false);

static void registerPass(const PassManagerBuilder &,
    legacy::PassManagerBase &PM) {
  auto p = new driver_pass();
  PM.add(p);
}

static RegisterStandardPasses RegisterPassOpt(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerPass);

static RegisterStandardPasses RegisterPassO0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);