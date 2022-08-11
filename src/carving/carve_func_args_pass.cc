#include"pass.hpp"

namespace {

class carver_pass : public ModulePass {

 public:
  static char ID;
  carver_pass() : ModulePass(ID) { func_id = 0;}

  bool runOnModule(Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "carving function argument instrumentation";

  }

 private:
  bool hookInstrs(Module &M);

  std::set<std::string> instrument_func_set;
  void get_instrument_func_set();
  
  void Insert_return_val_probe(Instruction *, std::string);
  
  void insert_global_carve_probe(Function * F, BasicBlock * BB);

  void gen_class_carver();

  int func_id;
};

}  // namespace

char carver_pass::ID = 0;

void carver_pass::gen_class_carver() {
  class_carver
    = Mod->getOrInsertFunction("__class_carver", VoidTy, Int8PtrTy, Int32Ty);
  Function * class_carver_func = dyn_cast<Function>(class_carver.getCallee());
  
  BasicBlock * entry_BB
    = BasicBlock::Create(*Context, "entry", class_carver_func);

  BasicBlock * default_BB
    = BasicBlock::Create(*Context, "default", class_carver_func);
  IRB->SetInsertPoint(default_BB);
  IRB->CreateRetVoid();

  IRB->SetInsertPoint(entry_BB);

  Value * carving_ptr = class_carver_func->getArg(0);
  Value * class_idx = class_carver_func->getArg(1);

  SwitchInst * switch_inst
    = IRB->CreateSwitch(class_idx, default_BB, num_class_name_const + 1);

  for (auto class_type : class_name_map) {
    int case_id = class_type.second.first;
    BasicBlock * case_block = BasicBlock::Create(*Context, std::to_string(case_id), class_carver_func);
    switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
    IRB->SetInsertPoint(case_block);

    StructType * class_type_ptr = class_type.first;
    
    Value * casted_var= IRB->CreateCast(Instruction::CastOps::BitCast
      , carving_ptr, PointerType::get(class_type_ptr, 0));

    insert_struct_carve_probe_inner(casted_var, class_type_ptr);
    IRB->CreateRetVoid();
  }

  //default is char *
  int case_id = num_class_name_const;
  BasicBlock * case_block = BasicBlock::Create(*Context, std::to_string(case_id), class_carver_func);
  switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
  IRB->SetInsertPoint(case_block);

  Value * load_val = IRB->CreateLoad(Int8Ty, carving_ptr);
  std::vector<Value *> probe_args {load_val};
  IRB->CreateCall(carv_char_func, probe_args);
  IRB->CreateRetVoid();

  switch_inst->setDefaultDest(case_block);

  default_BB->eraseFromParent();

  return;
}

void carver_pass::Insert_return_val_probe(Instruction * IN, std::string callee_name) {
  IRB->SetInsertPoint(IN->getNextNonDebugInstruction());
  
  Type * ret_type = IN->getType();
  if (ret_type != VoidTy) {
    Constant * name_const = gen_new_string_constant("\"" + callee_name + "\"_ret", IRB);
    std::vector<Value *> push_args {name_const};
    IRB->CreateCall(carv_name_push, push_args);

    insert_carve_probe(IN, IN->getParent());
    IRB->CreateCall(carv_name_pop, {});
  }
}

void carver_pass::insert_global_carve_probe(Function * F, BasicBlock * BB) {

  BasicBlock * cur_block = BB;

  auto search = global_var_uses.find(F);
  if (search != global_var_uses.end()) {
    for (auto glob_iter : search->second) {
      std::string glob_name = glob_iter->getName().str();

      Type * const_type = glob_iter->getType();
      assert(const_type->isPointerTy());
      Type * pointee_type = dyn_cast<PointerType>(const_type)->getPointerElementType();

      Constant * glob_name_const = gen_new_string_constant(glob_name, IRB);
      std::vector<Value *> push_args {glob_name_const};
      IRB->CreateCall(carv_name_push, push_args);

      if (pointee_type->isStructTy()) {
        insert_struct_carve_probe((Value *) glob_iter, pointee_type);
        IRB->CreateCall(carv_name_pop, {});
      } else {
        Value * load_val = IRB->CreateLoad(pointee_type, (Value *) glob_iter);
        cur_block = insert_carve_probe(load_val, cur_block);
        IRB->CreateCall(carv_name_pop, {});
      }
    }
  }

  return;
}

bool carver_pass::hookInstrs(Module &M) {
  initialize_pass_contexts(M);

  get_llvm_types();
  get_carving_func_callees();

  //Set dummy insertlocation...
  for (auto &F : Mod->functions()) {
    std::string func_name = F.getName().str();
    if (func_name == "main") {
      IRB->SetInsertPoint(F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
      break;
    }
  }

  get_class_type_info();

  get_instrument_func_set();

  find_global_var_uses();

  gen_class_carver();

  DEBUG0("Iterating functions...\n");

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) { continue; }
    if (&F == class_carver.getCallee()) { continue; }

    std::string func_name = F.getName().str();
    if (instrument_func_set.find(func_name) == instrument_func_set.end()) {
      std::vector<CallInst *> call_instrs;
      std::vector<ReturnInst *> ret_instrs;

      for (auto &BB : F) {
        for (auto &IN : BB) {
          if (isa<CallInst>(&IN)) {
            call_instrs.push_back(dyn_cast<CallInst>(&IN));
          } else if (isa<ReturnInst>(&IN)) {
            ret_instrs.push_back(dyn_cast<ReturnInst>(&IN));
          }
        }
      }

      BasicBlock& entry_block = F.getEntryBlock();
      Insert_alloca_probe(entry_block);

      //Perform memory tracking
      for (auto call_instr : call_instrs) {
        Function * callee = call_instr->getCalledFunction();
        if ((callee == NULL) || (callee->isDebugInfoForProfiling())) { continue; }
        std::string callee_name = callee->getName().str();
        Insert_mem_func_call_probe(call_instr, callee_name);
      }

      for (auto ret_instr : ret_instrs) {
        IRB->SetInsertPoint(ret_instr);

        //Remove alloca (local variable) memory tracking info.
        for (auto iter = tracking_allocas.begin();
          iter != tracking_allocas.end(); iter++) {
          AllocaInst * alloc_instr = *iter;

          Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, alloc_instr, Int8PtrTy);
          std::vector<Value *> args {casted_ptr};
          IRB->CreateCall(remove_probe, args);
        }
      }

      tracking_allocas.clear();
      continue;
    }

    DEBUG0("Inserting probe in " << func_name << "\n");

    std::vector<Instruction *> cast_instrs;
    std::vector<CallInst *> call_instrs;
    std::vector<Instruction *> ret_instrs;
    for (auto &BB : F) {
      for (auto &IN : BB) {
        if (isa<CastInst>(&IN)) {
          cast_instrs.push_back(&IN);
        } else if (isa<CallInst>(&IN)) {
          call_instrs.push_back(dyn_cast<CallInst>(&IN));
        } else if (isa<ReturnInst>(&IN)) {
          ret_instrs.push_back(&IN);
        }
      }
    }

    BasicBlock& entry_block = F.getEntryBlock();
    Insert_alloca_probe(entry_block);

    IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());
    Constant * func_id_const = ConstantInt::get(Int32Ty, func_id++);
    std::vector<Value *> func_call_args {func_id_const};
    Instruction * init_probe
      = IRB->CreateCall(carv_func_call, func_call_args);

    //Main argc argv handling
    if (func_name == "main") {
      Insert_carving_main_probe(entry_block, F, M.global_values());
      IRB->SetInsertPoint(init_probe->getNextNonDebugInstruction());
    } else if (F.isVarArg()) {
      //TODO
    } else {

      //Insert input carving probes
      int param_idx = 0;
  
      BasicBlock * insert_block = &entry_block;

      for (auto &arg_iter : F.args()) {
        Value * func_arg = &arg_iter;

        std::string param_name = find_param_name(func_arg, insert_block);

        if (param_name == "") {
          param_name = "parm_" + std::to_string(param_idx);
        }

        Constant * param_name_const = gen_new_string_constant(param_name, IRB);
        std::vector<Value *> push_args {param_name_const};
        IRB->CreateCall(carv_name_push, push_args);

        insert_block
          = insert_carve_probe(func_arg, insert_block);
        
        std::vector<Value *> pop_args;
        IRB->CreateCall(carv_name_pop, pop_args);
        param_idx ++;
      }

      insert_global_carve_probe(&F, insert_block);
    }

    IRB->CreateCall(update_carved_ptr_idx, {});

    DEBUG0("Insert memory tracking for " << func_name << "\n");

    //Call instr probing
    for (auto call_instr : call_instrs) {
      //insert new/free probe, return value probe
      Function * callee = call_instr->getCalledFunction();
      if ((callee == NULL) || (callee->isDebugInfoForProfiling())) { continue; }

      std::string callee_name = callee->getName().str();
      if (callee_name == "__cxa_allocate_exception") { continue; }

      if (callee_name == "__cxa_throw") {
        //exception handling
        IRB->SetInsertPoint(call_instr);
        Constant * func_name_const = gen_new_string_constant(func_name, IRB);
        std::vector<Value *> probe_args {func_name_const, func_id_const};
        IRB->CreateCall(carv_func_ret, probe_args);    

        //Remove alloca (local variable) memory tracking info.
        for (auto iter = tracking_allocas.begin();
          iter != tracking_allocas.end(); iter++) {
          AllocaInst * alloc_instr = *iter;

          Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, alloc_instr, Int8PtrTy);
          std::vector<Value *> args {casted_ptr};
          IRB->CreateCall(remove_probe, args);          
        }

        //Insert fini
        if (func_name == "main") {
          IRB->CreateCall(__carv_fini, std::vector<Value *>());
        }
      } else {
        Insert_mem_func_call_probe(call_instr, callee_name);
        Insert_return_val_probe(call_instr, callee_name);
      }
    }

    //Probing at return
    for (auto ret_instr : ret_instrs) {
      IRB->SetInsertPoint(ret_instr);

      //Write carved result
      Constant * func_name_const = gen_new_string_constant(func_name, IRB);

      std::vector<Value *> probe_args {func_name_const, func_id_const};
      IRB->CreateCall(carv_func_ret, probe_args);

      //Remove alloca (local variable) memory tracking info.
      for (auto iter = tracking_allocas.begin();
        iter != tracking_allocas.end(); iter++) {
        AllocaInst * alloc_instr = *iter;

        Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, alloc_instr, Int8PtrTy);
        std::vector<Value *> args {casted_ptr};
        IRB->CreateCall(remove_probe, args);
      }

      //Insert fini
      if (func_name == "main") {
        IRB->CreateCall(__carv_fini, std::vector<Value *>());
      }
    }

    tracking_allocas.clear();

    DEBUG0("done in " << func_name << "\n");
  }

  char * tmp = getenv("DUMP_IR");
  if (tmp) {
    M.dump();
  }

  delete IRB;
  return true;
}

bool carver_pass::runOnModule(Module &M) {

  DEBUG0("Running carver_pass\n");

  read_probe_list("carver_probe_names.txt");
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

void carver_pass::get_instrument_func_set() {

  std::ifstream targets("targets.txt");
  if (targets.good()) {
    DEBUG0("Reading targets from targets.txt\n");
    std::string line;
    while (std::getline(targets, line)) {
      if (line.length() == 0) { continue; }
      if (line[0] == '#') { continue; }
      instrument_func_set.insert(line);
    }

    instrument_func_set.insert("main");

    return;
  }

  std::ofstream outfile("funcs.txt");
  std::ofstream outfile2("funcs_size.txt");

  for (auto &F : Mod->functions()) {
    if (F.isIntrinsic() || !F.size()) { continue; }
    std::string func_name = F.getName().str();
    if (func_name == "_GLOBAL__sub_I_main.cc") { continue;}
    if (func_name == "__cxx_global_var_init") { continue; }
    GlobalValue::LinkageTypes Flinkage = F.getLinkage();
    
    if (!GlobalValue::isExternalLinkage(Flinkage)) { continue; }
    if (F.size() < 10) { continue; }

    //TODO
    if (F.isVarArg()) { continue; }
    
    for (auto iter : DbgFinder.subprograms()) {

      if ((iter->getLinkageName().str() == func_name) || (iter->getName().str() == func_name)) {
        std::string filename = iter->getFilename().str();
        if (filename.find("/usr/bin/") == std::string::npos) {
          outfile << func_name << "\n";
          instrument_func_set.insert(func_name);

          outfile2 << func_name << ":" << F.size() << ":" << filename << "\n";
        }
        break;
      }
    }
  }

  if (instrument_func_set.find("main") == instrument_func_set.end()) {
    instrument_func_set.insert("main");
    outfile << "main\n";
  }

  outfile.close();
  outfile2.close();
}

static void registercarver_passPass(const PassManagerBuilder &,
                                           legacy::PassManagerBase &PM) {

  auto p = new carver_pass();
  PM.add(p);

}

static RegisterStandardPasses Registercarver_passPass(
    PassManagerBuilder::EP_OptimizerLast, registercarver_passPass);

static RegisterStandardPasses Registercarver_passPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registercarver_passPass);

static RegisterStandardPasses Registercarver_passPassLTO(
    PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
    registercarver_passPass);

