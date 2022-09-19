#include "carve_pass.hpp"

namespace {

class carver_pass : public ModulePass {

public:
  static char ID;
  carver_pass() : ModulePass(ID) { func_id = 0; }

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

  void Insert_return_val_probe(Instruction *, Function *);

  void insert_global_carve_probe(Function *F, BasicBlock *BB);

  void gen_class_carver();

  int func_id;

  std::ofstream carved_types_file;
};

} // namespace

char carver_pass::ID = 0;

void carver_pass::gen_class_carver() {
  class_carver =
      Mod->getOrInsertFunction("__class_carver", VoidTy, Int8PtrTy, Int32Ty);
  Function *class_carver_func = dyn_cast<Function>(class_carver.getCallee());

  BasicBlock *entry_BB =
      BasicBlock::Create(*Context, "entry", class_carver_func);

  BasicBlock *default_BB =
      BasicBlock::Create(*Context, "default", class_carver_func);

  // Put return void ad default block
  IRB->SetInsertPoint(default_BB);
  IRB->CreateRetVoid();

  IRB->SetInsertPoint(entry_BB);

  Value *carving_ptr = class_carver_func->getArg(0); // *int8
  Value *class_idx = class_carver_func->getArg(1);   // int32

  SwitchInst *switch_inst =
      IRB->CreateSwitch(class_idx, default_BB, num_class_name_const + 1);

  for (auto class_type : class_name_map) {
    int case_id = class_type.second.first;
    BasicBlock *case_block = BasicBlock::Create(
        *Context, std::to_string(case_id), class_carver_func);
    switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
    IRB->SetInsertPoint(case_block);

    StructType *class_type_ptr = class_type.first;

    Value *casted_var =
        IRB->CreateCast(Instruction::CastOps::BitCast, carving_ptr,
                        PointerType::get(class_type_ptr, 0));

    insert_struct_carve_probe_inner(casted_var, class_type_ptr);
    IRB->CreateRetVoid();
  }

  // default is char *
  int case_id = num_class_name_const;
  BasicBlock *case_block =
      BasicBlock::Create(*Context, std::to_string(case_id), class_carver_func);
  switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
  IRB->SetInsertPoint(case_block);

  Value *load_val = IRB->CreateLoad(Int8Ty, carving_ptr);
  IRB->CreateCall(carv_char_func, {load_val});
  IRB->CreateRetVoid();

  switch_inst->setDefaultDest(case_block);

  default_BB->eraseFromParent();

  return;
}

void carver_pass::Insert_return_val_probe(Instruction *IN, Function *callee) {
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

    Constant *name_const =
        gen_new_string_constant("\"" + callee_name + "\"_ret", IRB);
    IRB->CreateCall(carv_name_push, {name_const});
    insert_carve_probe(IN, IRB->GetInsertBlock());
    IRB->CreateCall(carv_name_pop, {});
  }
}

void carver_pass::insert_global_carve_probe(Function *F, BasicBlock *BB) {

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
      Value *glob_val = IRB->CreateLoad(pointee_type, glob_iter);
      IRB->CreateCall(carv_name_push, {glob_name_const});
      cur_block = insert_carve_probe(glob_val, cur_block);
      IRB->CreateCall(carv_name_pop, {});

      carved_types_file << "**" << glob_name << " : "
                        << get_type_str(pointee_type) << "\n";
    }
  }

  return;
}

bool carver_pass::hookInstrs(Module &M) {
  initialize_pass_contexts(M);

  get_llvm_types();
  get_carving_func_callees();

  // Constructs global variables to global symbol table.
  global_carve_ready = Mod->getOrInsertGlobal("__carv_ready", Int8Ty);
  global_cur_class_idx =
      Mod->getOrInsertGlobal("__carv_cur_class_index", Int32Ty);
  global_cur_class_size =
      Mod->getOrInsertGlobal("__carv_cur_class_size", Int32Ty);

  get_class_type_info();

  get_instrument_func_set();

  find_global_var_uses();

  gen_class_carver();

  carved_types_file.open("carved_types.txt");

  DEBUG0("Iterating functions...\n");

  for (auto &F : M) {
    if (is_inst_forbid_func(&F)) {
      continue;
    }

    std::string func_name = F.getName().str();

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

    if (instrument_func_set.find(func_name) == instrument_func_set.end()) {
      // Just insert memory tracking probes
      BasicBlock &entry_block = F.getEntryBlock();
      Insert_alloca_probe(entry_block);

      // Perform memory tracking
      for (auto call_instr : call_instrs) {
        Function *callee = call_instr->getCalledFunction();
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
        Insert_mem_func_call_probe(call_instr, callee_name);
      }

      for (auto ret_instr : ret_instrs) {
        IRB->SetInsertPoint(ret_instr);
        insert_dealloc_probes();
      }

      tracking_allocas.clear();
      continue;
    }

    DEBUG0("Inserting probe in " << func_name << "\n");

    carved_types_file << "##" << func_name << "\n";

    BasicBlock &entry_block = F.getEntryBlock();
    Insert_alloca_probe(entry_block);

    IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());
    Constant *func_id_const = ConstantInt::get(Int32Ty, func_id++);
    Instruction *init_probe = IRB->CreateCall(carv_func_call, {func_id_const});

    // Main argc argv handling
    if (func_name == "main") {
      Insert_carving_main_probe(&entry_block, &F);
      tracking_allocas.clear();
      continue;
    } else if (F.isVarArg()) {
      // TODO, unreachable
    } else {

      // Insert input carving probes
      int param_idx = 0;

      insert_check_carve_ready();

      BasicBlock *insert_block = IRB->GetInsertBlock();

      for (auto &arg_iter : F.args()) {
        Value *func_arg = &arg_iter;

        std::string param_name = find_param_name(func_arg, insert_block);

        if (param_name == "") {
          param_name = "parm_" + std::to_string(param_idx);
        }

        carved_types_file << "**" << param_name << " : "
                          << get_type_str(func_arg->getType()) << "\n";

        Constant *param_name_const = gen_new_string_constant(param_name, IRB);
        IRB->CreateCall(carv_name_push, {param_name_const});

        insert_block = insert_carve_probe(func_arg, insert_block);

        IRB->CreateCall(carv_name_pop, {});
        param_idx++;
      }

      insert_global_carve_probe(&F, insert_block);
    }

    IRB->CreateCall(update_carved_ptr_idx, {});

    DEBUG0("Insert memory tracking for " << func_name << "\n");

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

      if (callee_name == "__cxa_throw") {
        // exception handling
        IRB->SetInsertPoint(call_instr);

        Constant *func_name_const = gen_new_string_constant(func_name, IRB);
        IRB->CreateCall(carv_func_ret, {func_name_const, func_id_const});

        insert_dealloc_probes();

        // Insert fini
        if (func_name == "main") {
          IRB->CreateCall(__carv_fini, {});
        }
      } else {
        Insert_mem_func_call_probe(call_instr, callee_name);
      }
    }

    // Probing at return
    for (auto ret_instr : ret_instrs) {
      IRB->SetInsertPoint(ret_instr);

      // Write carved result
      Constant *func_name_const = gen_new_string_constant(func_name, IRB);

      IRB->CreateCall(carv_func_ret, {func_name_const, func_id_const});

      insert_dealloc_probes();
    }

    tracking_allocas.clear();

    DEBUG0("done in " << func_name << "\n");
  }

  carved_types_file.close();

  char *tmp = getenv("DUMP_IR");
  if (tmp) {
    M.dump();
  }

  delete IRB;
  return true;
}

// Runs at module startup
bool carver_pass::runOnModule(Module &M) {

  DEBUG0("Running carver_pass\n");

  read_probe_list("carver_probe_names.txt");
  hookInstrs(M);

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

// Analyze existing functions and write on func_types.txt and target_funcs.txt.
void carver_pass::get_instrument_func_set() {
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
    if (func_name == "__cxx_global_var_init") {
      continue;
    }
    if (func_name.find("llvm_gcov") != std::string::npos) {
      continue;
    }

    // TODO
    if (F.isVarArg()) {
      continue;
    }

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

static RegisterPass<carver_pass> X("carve", "Carve pass", false, false);

static void registerPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {

  auto p = new carver_pass();
  PM.add(p);
}

static RegisterStandardPasses
    RegisterPassOpt(PassManagerBuilder::EP_ModuleOptimizerEarly, registerPass);

static RegisterStandardPasses
    RegisterPassO0(PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);

// static RegisterStandardPasses RegisterPassLTO(
//     PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
//     registerPass);
