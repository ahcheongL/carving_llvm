#include "carving/carve_pass.hpp"

/* Usage
  1. Set target type names in target.txt
  2. Execute the executable as usual,
     but add output directory at the end of the command.
*/

#define FIELD_THRESHOLD 10

namespace {

class type_carver_pass : public ModulePass {
 public:
  static char ID;
  type_carver_pass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    DEBUG0("Running type_carver_pass\n");

    initialize_pass_contexts(M);

    get_llvm_types();

    for (auto &F : M) {
      func_list.push_back(&F);
    }

    get_carving_func_callees_and_globals(false);

    if (!get_target_types()) {
      DEBUG0("No target type found\n");
      return false;
    }

    DEBUG0("Found " << target_types.size() << " target types\n");

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
    return "carving complex types instrumentation";
  }

 private:
  bool instrument_module();

  bool get_target_types();
  bool is_complex_type(Type *);

  int func_id = 0;

  std::vector<Function *> func_list;

  std::set<Type *> target_types;
  std::string target_type_name;
};

}  // namespace

char type_carver_pass::ID = 0;

bool type_carver_pass::instrument_module() {
  get_class_type_info();

  find_global_var_uses();

  construct_ditype_map();

  gen_class_carver();

  size_t num_alloca_instrs = 0;

  DEBUG0("Iterating functions...\n");

  Function *main_func = NULL;

  for (auto &F : Mod->functions()) {
    if (is_inst_forbid_func(&F)) {
      continue;
    }
    std::string func_name = F.getName().str();

    std::vector<Instruction *> cast_instrs;
    std::vector<CallInst *> call_instrs;
    std::vector<ReturnInst *> ret_instrs;
    for (auto &BB : F) {
      for (auto &IN : BB) {
        if (isa<CastInst>(&IN)) {
          cast_instrs.push_back(&IN);
        } else if (isa<CallInst>(&IN)) {
          call_instrs.push_back(dyn_cast<CallInst>(&IN));
        } else if (isa<ReturnInst>(&IN)) {
          ret_instrs.push_back(dyn_cast<ReturnInst>(&IN));
        }
      }
    }

    BasicBlock &entry_block = F.getEntryBlock();
    Insert_alloca_probe(entry_block);

    // Main argc argv handling
    if (func_name == "main") {
      Insert_carving_main_probe(&entry_block, &F, &func_list);
      main_func = &F;
    }

    // Call instr probing
    for (auto call_instr : call_instrs) {
      // insert new/free probe, return value probe
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

        // Remove alloca (local variable) memory tracking info.
        insert_dealloc_probes();

        // Insert fini
        if (func_name == "main") {
          IRB->CreateCall(__carv_fini, {});
        }
        continue;
      }

      IRB->SetInsertPoint(call_instr->getNextNonDebugInstruction());
      Insert_mem_func_call_probe(call_instr, callee_name);
    }

    // Probing at return
    for (auto ret_instr : ret_instrs) {
      IRB->SetInsertPoint(ret_instr);

      // Remove alloca (local variable) memory tracking info.
      for (auto iter = tracking_allocas.begin(); iter != tracking_allocas.end();
           iter++) {
        AllocaInst *alloc_instr = *iter;

        Value *casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast,
                                            alloc_instr, Int8PtrTy);
        IRB->CreateCall(remove_probe, {casted_ptr});
      }

      // Insert fini
      if (func_name == "main") {
        IRB->CreateCall(__carv_fini, {});
      }
    }

    num_alloca_instrs += tracking_allocas.size();

    tracking_allocas.clear();

    std::set<BasicBlock *> bbs;

    for (auto &BB : F) {
      bbs.insert(&BB);
    }

    int num_inserted = 0;

    for (auto &BB : bbs) {
      for (auto IN = BB->rbegin(); IN != BB->rend(); IN++) {
        Type *instr_type = IN->getType();

        if (target_types.find(instr_type) == target_types.end()) {
          continue;
        }

        Constant *name = gen_new_string_constant("obj", IRB);

        Instruction *tmp = &(*IN);
        while (tmp->getNextNonDebugInstruction() &&
               isa<PHINode>(tmp->getNextNonDebugInstruction())) {
          tmp = tmp->getNextNonDebugInstruction();
        }

        IRB->SetInsertPoint(tmp->getNextNonDebugInstruction());

        IRB->CreateCall(carv_open, {});

        // IRB->CreateCall(carv_name_push, {name});

        insert_carve_probe(&*IN, BB);

        // IRB->CreateCall(carv_name_pop, {});

        Constant *type_name_const =
            gen_new_string_constant(target_type_name, IRB);
        Constant *func_name_const = gen_new_string_constant(func_name, IRB);
        IRB->CreateCall(carv_close, {type_name_const, func_name_const});

        num_inserted++;
        break;
      }
    }

    if (num_inserted) {
      IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());
      Constant *func_id_const = ConstantInt::get(Int32Ty, func_id++);
      Instruction *init_probe =
          IRB->CreateCall(carv_func_call, {func_id_const});

      DEBUG0("Inserted " << num_inserted << " carving probes in function "
                         << func_name << "\n");
    }
  }

  check_and_dump_module();

  delete IRB;

  if (main_func == NULL) {
    DEBUG0("No main function found, carving won't work.\n");
  }
  return true;
}

bool type_carver_pass::get_target_types() {
  std::ifstream target_f("target.txt");
  if (!target_f.good()) {
    DEBUG0("No target.txt found, carving won't work.\n");
    return false;
  }

  std::string line;
  while (std::getline(target_f, line)) {
    if (line.length() == 0) {
      continue;
    }
    if (line[0] == '#') {
      continue;
    }
    target_type_name = line;
    break;
  }

  DEBUG0("Target type name: " << target_type_name << "\n");
  if (target_type_name.substr(0, 7) != "struct.") {
    target_type_name = "struct." + target_type_name;
  }

  for (auto &T : Mod->getIdentifiedStructTypes()) {
    std::string type_name = T->getName().str();
    if (type_name.substr(0, 6) == "union.") {
      continue;
    }
    auto search = type_name.substr(7).find(".");
    if (search != std::string::npos) {
      type_name = type_name.substr(0, search + 7);
    }
    if (type_name == target_type_name) {
      Type *ptr_type = PointerType::get(T, 0);
      target_types.insert(ptr_type);
    }
  }

  return target_types.size() > 0;
}

static void register_pass(const PassManagerBuilder &,
                          legacy::PassManagerBase &PM) {
  PM.add(new type_carver_pass());
}

static RegisterStandardPasses Y(PassManagerBuilder::EP_ModuleOptimizerEarly,
                                register_pass);

static RegisterStandardPasses Y0(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                 register_pass);
