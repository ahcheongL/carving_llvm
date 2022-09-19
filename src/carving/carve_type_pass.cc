#include "carve_pass.hpp"

#define FIELD_THRESHOLD 10

namespace {

class carver_pass : public ModulePass {

public:
  static char ID;
  carver_pass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "carving complex types instrumentation";
  }

private:
  bool hookInstrs(Module &M);

  std::set<StructType *> complex_types;
  void get_complex_types();
  bool is_complex_type(Type *);
  std::string get_type_name(Type *);

  void gen_class_carver();

  FunctionCallee carv_open;
  FunctionCallee carv_close;
};

} // namespace

char carver_pass::ID = 0;

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

bool carver_pass::hookInstrs(Module &M) {
  initialize_pass_contexts(M);

  get_llvm_types();
  get_carving_func_callees();

  carv_open = M.getOrInsertFunction(get_link_name("__carv_open"), VoidTy);
  carv_close = M.getOrInsertFunction(get_link_name("__carv_close"), VoidTy,
                                     Int8PtrTy, Int8PtrTy);

  size_t num_funcs = 0;
  size_t num_instrs = 0;
  for (auto &F : M) {
    for (auto &BB : F) {
      num_instrs += BB.size();
    }
    num_funcs++;
  }
  DEBUG0("# of functions: " << num_funcs << "\n");
  DEBUG0("# of instructions : " << num_instrs << "\n");

  get_class_type_info();

  get_complex_types();

  find_global_var_uses();

  construct_ditype_map();

  gen_class_carver();

  size_t num_alloca_instrs = 0;

  DEBUG0("Iterating functions...\n");

  Function *main_func = NULL;

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) {
      continue;
    }
    if (&F == class_carver.getCallee()) {
      continue;
    }

    std::string func_name = F.getName().str();

    if (func_name.substr(0, 7) == "__Carv_") {
      continue;
    }
    if (func_name.find("__cxx_global_var_init") != std::string::npos) {
      continue;
    }
    if (func_name.find("_GLOBAL__sub_I_") != std::string::npos) {
      continue;
    }

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
      Insert_carving_main_probe(&entry_block, &F);
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
        for (auto iter = tracking_allocas.begin();
             iter != tracking_allocas.end(); iter++) {
          AllocaInst *alloc_instr = *iter;

          Value *casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast,
                                              alloc_instr, Int8PtrTy);
          IRB->CreateCall(remove_probe, {casted_ptr});
        }

        // Insert fini
        if (func_name == "main") {
          IRB->CreateCall(__carv_fini, std::vector<Value *>());
        }
        continue;
      }

      bool mem_funcs = Insert_mem_func_call_probe(call_instr, callee_name);
      if (mem_funcs) {
        continue;
      }

      IRB->SetInsertPoint(call_instr->getNextNonDebugInstruction());

      int operand_index = 0;

      BasicBlock *insert_block = call_instr->getParent();
      for (auto &arg_iter : call_instr->arg_operands()) {
        Value *func_arg = arg_iter;
        Type *func_arg_type = func_arg->getType();
        bool to_carve = is_complex_type(func_arg_type);
        if (!to_carve) {
          operand_index++;
          continue;
        }

        std::string type_name = get_type_name(func_arg_type);
        if (type_name == "") {
          continue;
        }

        IRB->CreateCall(carv_open, {});

        std::string operand_name = "op_" + std::to_string(operand_index);
        Constant *operand_name_const =
            gen_new_string_constant(operand_name, IRB);
        IRB->CreateCall(carv_name_push, {operand_name_const});

        insert_block = insert_carve_probe(func_arg, insert_block);

        IRB->CreateCall(carv_name_pop, {});

        Constant *type_name_const = gen_new_string_constant(type_name, IRB);
        Constant *func_name_const = gen_new_string_constant(func_name, IRB);
        IRB->CreateCall(carv_close, {type_name_const, func_name_const});
        operand_index++;
      }
    }

    // Probing at return
    for (auto ret_instr : ret_instrs) {
      IRB->SetInsertPoint(ret_instr);

      // IRB->CreateCall(carv_dealloc_time_begin, {});
      // Remove alloca (local variable) memory tracking info.
      for (auto iter = tracking_allocas.begin(); iter != tracking_allocas.end();
           iter++) {
        AllocaInst *alloc_instr = *iter;

        Value *casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast,
                                            alloc_instr, Int8PtrTy);
        IRB->CreateCall(remove_probe, {casted_ptr});
      }
      // IRB->CreateCall(carv_dealloc_time_end, {});

      // Insert fini
      if (func_name == "main") {
        IRB->CreateCall(__carv_fini, std::vector<Value *>());
      }
    }

    num_alloca_instrs += tracking_allocas.size();

    tracking_allocas.clear();
  }

  char *tmp = getenv("DUMP_IR");
  if (tmp) {
    M.dump();
  }

  num_instrs = 0;
  num_funcs = 0;
  for (auto &F : M) {
    for (auto &BB : F) {
      num_instrs += BB.size();
    }
    num_funcs++;
  }
  DEBUG0("# of functions: " << num_funcs << "\n");
  DEBUG0("# of instructions : " << num_instrs << "\n");
  DEBUG0("# of alloca instructions : " << num_alloca_instrs * 2 << "\n");

  delete IRB;

  if (main_func == NULL) {
    DEBUG0("No main function found, carving won't work.\n");
  }
  return true;
}

void carver_pass::gen_class_carver() {
  class_carver =
      Mod->getOrInsertFunction("__class_carver", VoidTy, Int8PtrTy, Int32Ty);
  Function *class_carver_func = dyn_cast<Function>(class_carver.getCallee());

  BasicBlock *entry_BB =
      BasicBlock::Create(*Context, "entry", class_carver_func);

  BasicBlock *default_BB =
      BasicBlock::Create(*Context, "default", class_carver_func);
  IRB->SetInsertPoint(default_BB);
  IRB->CreateRetVoid();

  IRB->SetInsertPoint(entry_BB);

  Value *carving_ptr = class_carver_func->getArg(0);
  Value *class_idx = class_carver_func->getArg(1);

  SwitchInst *switch_inst =
      IRB->CreateSwitch(class_idx, default_BB, num_class_name_const + 1);

  DEBUG0("Make carver of structs, # of structs : " << num_class_name_const
                                                   << "\n");

  unsigned int idx = 0;
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

    idx++;
    if (idx % 100 == 0) {
      DEBUG0(idx << "/" << num_class_name_const << " done\n");
    }
  }

  DEBUG0("Done\n");

  // default is char *
  int case_id = num_class_name_const;
  BasicBlock *case_block =
      BasicBlock::Create(*Context, std::to_string(case_id), class_carver_func);
  switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
  IRB->SetInsertPoint(case_block);

  Value *load_val = IRB->CreateLoad(Int8Ty, carving_ptr);
  std::vector<Value *> probe_args{load_val};
  IRB->CreateCall(carv_char_func, probe_args);
  IRB->CreateRetVoid();

  switch_inst->setDefaultDest(case_block);

  default_BB->eraseFromParent();

  return;
}

static unsigned int get_num_fields(StructType *type);
static bool exclude_struct(std::string name);

void carver_pass::get_complex_types() {

  bool changed = true;

  while (changed) {
    changed = false;
    for (auto struct_type : Mod->getIdentifiedStructTypes()) {
      if (struct_type->isOpaque()) {
        continue;
      }
      if (complex_types.find(struct_type) != complex_types.end()) {
        continue;
      }

      std::string name = struct_type->getName().str();
      unsigned int num_fields = get_num_fields(struct_type);

      bool is_boost = true; // name.find("boost") != std::string::npos;

      if (exclude_struct(name)) {
        continue;
      }

      if ((num_fields > FIELD_THRESHOLD) && is_boost) {
        complex_types.insert(struct_type);
        changed = true;
      } else {
        for (auto field_type : struct_type->elements()) {

          while (field_type->isPointerTy()) {
            field_type = field_type->getPointerElementType();
          }

          while (field_type->isArrayTy()) {
            field_type = field_type->getArrayElementType();
          }

          if (!field_type->isStructTy()) {
            continue;
          }

          StructType *field_struct_type = dyn_cast<StructType>(field_type);
          if (complex_types.find(field_struct_type) == complex_types.end()) {
            continue;
          }
          complex_types.insert(struct_type);
          changed = true;
        }
      }
    }
  }

  std::ofstream out_file;
  out_file.open("complex_types.txt");
  for (auto type : complex_types) {
    out_file << type->getName().str() << "\n";
  }
  out_file.close();
}

static unsigned int get_num_fields(StructType *type) {
  unsigned int num_fields = 0;
  for (unsigned int i = 0; i < type->getNumElements(); i++) {
    if (type->getElementType(i)->isStructTy()) {
      num_fields +=
          get_num_fields(dyn_cast<StructType>(type->getElementType(i)));
    } else {
      num_fields++;
    }
  }
  return num_fields;
}

static bool exclude_struct(std::string name) {
  if (name.find("std::") != std::string::npos) {
    return true;
  }
  if (name.find("struct._IO_FILE") != std::string::npos) {
    return true;
  }
  if (name.find(".anon.") != std::string::npos) {
    return true;
  }
  if (name.find("__gnu_cxx::") != std::string::npos) {
    return true;
  }

  if (name.find("google::protobuf") != std::string::npos) {
    return true;
  }
  if (name.find("testing::") != std::string::npos) {
    return true;
  }

  size_t name_len = name.size();
  if ((name_len > 4) && (name.substr(name_len - 4) == "Test")) {
    return true;
  }
  if ((name_len > 9) && (name.substr(name_len - 9) == "Test.base")) {
    return true;
  }

  if (name.find("opencv_test") != std::string::npos) {
    return true;
  }

  return false;
}

bool carver_pass::is_complex_type(Type *type) {
  if (type->isStructTy()) {
    StructType *struct_type = dyn_cast<StructType>(type);
    if (complex_types.find(struct_type) != complex_types.end()) {
      return true;
    }
  } else if (type->isPointerTy()) {
    PointerType *ptr_type = dyn_cast<PointerType>(type);
    return is_complex_type(ptr_type->getElementType());
  } else if (type->isArrayTy()) {
    ArrayType *array_type = dyn_cast<ArrayType>(type);
    return is_complex_type(array_type->getElementType());
  }
  return false;
}

std::string carver_pass::get_type_name(Type *type) {
  if (type->isStructTy()) {
    StructType *struct_type = dyn_cast<StructType>(type);
    if (struct_type->isOpaque()) {
      return "";
    }
    return struct_type->getName().str();
  } else if (type->isPointerTy()) {
    PointerType *ptr_type = dyn_cast<PointerType>(type);
    if (ptr_type->getElementType()->isStructTy()) {
      StructType *struct_type =
          dyn_cast<StructType>(ptr_type->getElementType());
      if (struct_type->isOpaque()) {
        return "";
      }
      return struct_type->getName().str() + "*";
    }
  }
  return "";
}

static void registercarver_passPass(const PassManagerBuilder &,
                                    legacy::PassManagerBase &PM) {

  auto p = new carver_pass();
  PM.add(p);
}

static RegisterStandardPasses
    Registercarver_passPass(PassManagerBuilder::EP_OptimizerLast,
                            registercarver_passPass);

static RegisterStandardPasses
    Registercarver_passPass0(PassManagerBuilder::EP_EnabledOnOptLevel0,
                             registercarver_passPass);

static RegisterStandardPasses Registercarver_passPassLTO(
    PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
    registercarver_passPass);
