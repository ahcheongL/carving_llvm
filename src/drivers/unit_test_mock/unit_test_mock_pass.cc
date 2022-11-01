#include "driver_pass.hpp"

namespace {

class driver_pass : public ModulePass {

 public:
  static char ID;
  driver_pass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "instrumenting to make replay driver";
  }

 private:
  bool hookInstrs(Module &M);

  std::set<Function *> target_funcs;
  std::set<std::string> public_funcs;

  bool get_target_funcs();
  bool get_public_use_funcs();

  void instrument_main_func(Function * main_func);
  void instrument_unit_test_body(Function *);
};

}  // namespace

char driver_pass::ID = 0;

bool driver_pass::hookInstrs(Module &M) {
  initialize_pass_contexts(M);
  get_llvm_types();

  get_driver_func_callees();

  bool res = get_target_funcs();
  if (res == false) {
    DEBUG0("get_target_func failed\n");
    return true;
  }

  res = get_public_use_funcs();
  if (res == false) {
    DEBUG0("get_public_use_funcs failed\n");
    return true;
  }

  get_class_type_info();

  gen_class_replay();

  for (auto &F : M) {
    if (is_inst_forbid_func(&F)) { continue; }
    std::string func_name = F.getName().str();

    if (func_name == "main") {
      instrument_main_func(&F);
      continue;
    }

    if (target_funcs.find(&F) == target_funcs.end()) { continue; }

    instrument_unit_test_body(&F);
  }
  
  char * tmp = getenv("DUMP_IR");
  if (tmp) {
    DEBUG0("Dumping IR...\n");
    DEBUGDUMP(Mod);
  }

  delete IRB;
  return true;
}

void driver_pass::instrument_main_func(Function * main_func) {

  const DebugLoc * debug_loc = NULL;

  //get random debug info...
  for (auto &BB : main_func->getBasicBlockList()) {
    for (auto &IN : BB) {
      if (isa<CallInst>(&IN)) {
        CallInst * CI = cast<CallInst>(&IN);
        const DebugLoc & DL = CI->getDebugLoc();
        debug_loc = &DL;
        break;
      }
    }

    if (debug_loc != NULL) { break; }
  }

  IRB->SetInsertPoint(main_func->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());

  //Record func ptr
  for (auto &Func : Mod->functions()) {
    if (Func.isIntrinsic()) { continue; }
    std::string func_name = Func.getName().str();
    if (func_name.find("__Replay__") != std::string::npos) { continue; }

    Constant * func_name_const = gen_new_string_constant(func_name, IRB);
    Value * cast_val = IRB->CreateCast(Instruction::CastOps::BitCast
      , &Func, Int8PtrTy);
    IRB->CreateCall(record_func_ptr, {cast_val, func_name_const});
  }

  //Record class type string constants
  for (auto iter : class_name_map) {
    unsigned int class_size = DL->getTypeAllocSize(iter.first);
    IRB->CreateCall(keep_class_info, {iter.second.second
      , ConstantInt::get(Int32Ty, class_size)
      , ConstantInt::get(Int32Ty, iter.second.first)});
  }

  Value * argc = main_func->getArg(0);
  Value * argv = main_func->getArg(1);
  AllocaInst * argc_ptr = IRB->CreateAlloca(Int32Ty);
  AllocaInst * argv_ptr = IRB->CreateAlloca(Int8PtrPtrTy);

  Value * new_argc = IRB->CreateLoad(Int32Ty, argc_ptr);
  Value * new_argv = IRB->CreateLoad(Int8PtrPtrTy, argv_ptr);

  argc->replaceAllUsesWith(new_argc);
  argv->replaceAllUsesWith(new_argv);

  IRB->SetInsertPoint((Instruction *) new_argc);

  IRB->CreateStore(argc, argc_ptr);
  IRB->CreateStore(argv, argv_ptr);
  
  FunctionCallee driver_input_modifier
    = Mod->getOrInsertFunction(get_link_name("__driver_input_modifier"), VoidTy, Int32PtrTy, Int8PtrPtrPtrTy);

  IRB->CreateCall(driver_input_modifier, {argc_ptr, argv_ptr});


  //Value * argv = main_func->getArg(1);
  //IRB->CreateCall(__inputf_open, {argv});
}

void driver_pass::instrument_unit_test_body(Function * func) {

  CallBase * api_call = NULL;

  std::string func_name = func->getName().str();

  for (auto &BB : func->getBasicBlockList()) {
    for (auto &IN : BB) {
      if (isa<CallBase>(&IN)) {
        CallBase * tmp_call_instr = dyn_cast<CallBase>(&IN);
        Function * callee = tmp_call_instr->getCalledFunction();
        if (callee == NULL) { continue; }
        if (callee->arg_size() == 0) { continue; }
        std::string callee_name = callee->getName().str();
        if (public_funcs.find(callee_name) == public_funcs.end()) { continue; }

        api_call = tmp_call_instr;
        break;
      }
    }
    if (api_call != NULL) { break; }
  }

  if (api_call == NULL) {
    DEBUG0("No api call found in " << func->getName().str() << "\n");
    return;
  }

  IRB->SetInsertPoint(api_call);

  BasicBlock * orig_block = api_call->getParent();

  BasicBlock * end_block = orig_block->splitBasicBlock(api_call);

  BasicBlock * new_block = BasicBlock::Create(api_call->getContext(), "replay_block", func);

  IRB->SetInsertPoint(new_block);

  Function * callee = api_call->getCalledFunction();
  
  Type * type_to_replace = NULL;
  int to_replace_idx = 0;

  int arg_idx = 0;
  for (auto &arg : callee->args()) {
    Type * arg_type = arg.getType();
    Value * replay_res = insert_replay_probe(arg_type, NULL);

    if ((type_to_replace == NULL)
      || ((!type_to_replace->isPointerTy()) && !is_func_ptr_type(arg_type))) {
      type_to_replace = arg_type;
      to_replace_idx = arg_idx;      
    }

    api_call->setArgOperand(arg_idx, replay_res);
    arg_idx ++;
  }

  IRB->CreateBr(end_block);

  orig_block->getTerminator()->setOperand(0, new_block);

  // DEBUG0("Replace: " << func_name << " : " << callee->getName().str() << " : ");
  // new_args[to_replace_idx]->getType()->print(llvm::errs());
  //  DEBUG0("\n");

  DEBUG0("Replace: " << func_name << " : " << callee->getName().str() << " : \n");

  return;
}

bool driver_pass::runOnModule(Module &M) {

  DEBUG0("Running binary fuzz driver_pass\n");

  read_probe_list("driver_probe_names.txt");
  read_probe_list("unit_test_probe_names.txt");
  hookInstrs(M);

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

bool driver_pass::get_target_funcs() {

  for (auto &F : Mod->functions()) {
    if (F.isIntrinsic() || !F.size()) { continue; }
    std::string func_name = F.getName().str();    
    
    if (func_name.find("TestBody") == std::string::npos) { continue; }
    target_funcs.insert(&F);
  }

  if (target_funcs.size() == 0) {
    DEBUG0("FATAL : No target function found.\n");
    return false;
  }

  return true;
}

bool driver_pass::get_public_use_funcs() {

  std::ifstream targets("public_funcs.txt");
  if (targets.good()) {
    DEBUG0("Reading targets from public_funcs.txt\n");
    std::string line;
    while (std::getline(targets, line)) {
      if (line.length() == 0) { continue; }
      if (line[0] == '#') { continue; }
      public_funcs.insert(line);
    }

    return public_funcs.size() > 0;
  }

  return false;
}

static RegisterPass<driver_pass> X("driver", "Driver pass", false , false);

static void registerPass(const PassManagerBuilder &,
    legacy::PassManagerBase &PM) {
  auto p = new driver_pass();
  PM.add(p);
}

static RegisterStandardPasses RegisterPassOpt(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerPass);

static RegisterStandardPasses RegisterPassO0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);