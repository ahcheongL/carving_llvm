#include "carving/carve_pass.hpp"
#include "llvm/Demangle/Demangle.h"

namespace {

class carver_pass : public ModulePass {
 public:
  static char ID;
  carver_pass() : ModulePass(ID) { func_id = 0; }

  bool runOnModule(Module &M) override {
    DEBUG0("Running carver_pass\n");

    read_probe_list("carver_probe_names.txt");
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
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "carving function argument instrumentation";
  }

 private:
  bool instrument_module();

  // Target function including main
  std::set<std::string> instrument_func_set;
  void get_instrument_func_set();

  void Insert_return_val_probe(Instruction *, Function *);

  void insert_global_carve_probe(Function *F, BasicBlock *BB);

  int func_id;

  std::ofstream carved_types_file;
};

}  // namespace

char carver_pass::ID = 0;

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

bool carver_pass::instrument_module() {
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

    // Just insert memory tracking probes
    BasicBlock &entry_block = F.getEntryBlock();
    Insert_alloca_probe(entry_block);

    if (instrument_func_set.find(func_name) == instrument_func_set.end()) {
      // Perform memory tracking
      for (llvm::CallInst *call_instr : call_instrs) {
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

        IRB->SetInsertPoint(call_instr->getNextNonDebugInstruction());
        Insert_mem_func_call_probe(call_instr, callee_name);
      }

      for (auto ret_instr : ret_instrs) {
        IRB->SetInsertPoint(ret_instr);
        insert_dealloc_probes();
      }

      tracking_allocas.clear();
      continue;
    }

    DEBUG0("Inserting probe in " << func_name << '\n');

    std::string demangled_func_name = llvm::demangle(func_name);
    carved_types_file << "##" << demangled_func_name << '\n';

    IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());
    Constant *func_id_const = ConstantInt::get(Int32Ty, func_id++);
    Instruction *init_probe = IRB->CreateCall(carv_func_call, {func_id_const});

    // Main argc argv handling
    if (demangled_func_name == "main") {
      Insert_carving_main_probe(&entry_block, &F, NULL);
      tracking_allocas.clear();
      continue;
    } else if (F.isVarArg()) {
      // TODO, unreachable
    } else {
      // Insert input carving probes
      int param_idx = 0;

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

static RegisterPass<carver_pass> X("carve", "Carve pass", false, false);

static void registerPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  auto p = new carver_pass();
  PM.add(p);
}

static RegisterStandardPasses RegisterPassOpt(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerPass);

static RegisterStandardPasses RegisterPassO0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);

// static RegisterStandardPasses RegisterPassLTO(
//     PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
//     registerPass);
