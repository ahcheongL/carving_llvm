#include "drivers/driver_pass.hpp"

namespace {

class driver_pass : public ModulePass {
 public:
  static char ID;
  driver_pass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    DEBUG0("Running binary fuzz driver_pass\n");
    
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

#if LLVM_VERSION_MAJOR >= 4
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "instrumenting to make replay driver";
  }

 private:
  bool hookInstrs(Module &M);

  Function *driver_func = NULL;
  Function *main_func = NULL;
  GlobalVariable *use_carved_obj = NULL;

  bool get_driver_func();
  bool get_insert_val();

  void instrument_driver_func();
};

}  // namespace

char driver_pass::ID = 0;

bool driver_pass::hookInstrs(Module &M) {
  initialize_pass_contexts(M);
  get_llvm_types();

  get_driver_func_callees();

  bool res = get_driver_func();
  if (res == false) {
    DEBUG0("get_driver_func failed\n");
    return true;
  }

  res = get_insert_val();
  if (res == false) {
    DEBUG0("get_insert_val failed\n");
    return true;
  }

  get_class_type_info();

  gen_class_replay();

  instrument_driver_func();

  char *tmp = getenv("DUMP_IR");
  if (tmp) {
    DEBUG0("Dumping IR...\n");
    DEBUGDUMP(Mod);
  }

  delete IRB;
  return true;
}

void driver_pass::instrument_driver_func() {
  int num_args = driver_func->arg_size();
  assert(num_args == 2);
  Value *data_arg = driver_func->getArg(0);
  Type *data_arg_type = data_arg->getType();
  assert(data_arg_type->isPointerTy());
  Type *data_arg_pointee_type = data_arg_type->getPointerElementType();
  assert(data_arg_pointee_type->isIntegerTy());

  Value *orig_obj_val = NULL;

  Instruction *val_insert_point = NULL;
  // remove old use_carved_obj assignment
  for (auto &BB : *driver_func) {
    for (auto &I : BB) {
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        Value *store_val = SI->getPointerOperand();
        if (store_val == use_carved_obj) {
          orig_obj_val = SI->getValueOperand();
          val_insert_point = SI->getNextNonDebugInstruction();
          SI->eraseFromParent();
          break;
        }
      }
    }
  }

  if (orig_obj_val == NULL) {
    return;
  }

  IRB->SetInsertPoint(
      driver_func->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());

  Value *size_arg = driver_func->getArg(1);
  Type *size_arg_type = size_arg->getType();

  AllocaInst *new_data_ptr = IRB->CreateAlloca(Int8PtrTy);
  AllocaInst *new_size_ptr = IRB->CreateAlloca(size_arg_type);

  LoadInst *new_data = IRB->CreateLoad(Int8PtrTy, new_data_ptr);
  LoadInst *new_size = IRB->CreateLoad(size_arg_type, new_size_ptr);

  data_arg->replaceAllUsesWith(new_data);
  size_arg->replaceAllUsesWith(new_size);

  IRB->SetInsertPoint(new_data);

  Value *data_1_arg =
      IRB->CreateInBoundsGEP(Int8Ty, data_arg, ConstantInt::get(Int32Ty, 4));

  IRB->CreateStore(data_1_arg, new_data_ptr);
  Value *new_size_val =
      IRB->CreateSub(size_arg, ConstantInt::get(size_arg_type, 4));
  IRB->CreateStore(new_size_val, new_size_ptr);

  IRB->SetInsertPoint(new_size->getNextNonDebugInstruction());

  // Record class type string constants
  for (auto iter : class_name_map) {
    unsigned int class_size = DL->getTypeAllocSize(iter.first);
    IRB->CreateCall(keep_class_info,
                    {iter.second.second, ConstantInt::get(Int32Ty, class_size),
                     ConstantInt::get(Int32Ty, iter.second.first)});
  }

  FunctionCallee read_carv_file = Mod->getOrInsertFunction(
      get_link_name("read_carv_file"), Int8Ty, Int8PtrTy, Int32Ty);

  if (size_arg_type != Int32Ty) {
    size_arg = IRB->CreateIntCast(size_arg, Int32Ty, false);
  }

  Value *read_val = IRB->CreateCall(read_carv_file, {data_arg, size_arg});

  IRB->SetInsertPoint(val_insert_point);

  Value *cmp_val = IRB->CreateICmpEQ(read_val, ConstantInt::get(Int8Ty, 0));

  BasicBlock *cur_block = IRB->GetInsertBlock();

  BasicBlock *end_block = cur_block->splitBasicBlock(&(*IRB->GetInsertPoint()));

  BasicBlock *then_block =
      BasicBlock::Create(Mod->getContext(), "then", driver_func, end_block);

  BasicBlock *new_block = BasicBlock::Create(Mod->getContext(), "new_block",
                                             driver_func, end_block);

  Instruction *new_term = IRB->CreateCondBr(cmp_val, then_block, new_block);
  new_term->removeFromParent();

  ReplaceInstWithInst(cur_block->getTerminator(), new_term);

  IRB->SetInsertPoint(then_block);

  IRB->CreateStore(orig_obj_val, use_carved_obj);

  IRB->CreateBr(end_block);

  IRB->SetInsertPoint(new_block);

  Type *insert_type = use_carved_obj->getType()->getPointerElementType();

  Value *insert_val = insert_replay_probe(insert_type, NULL);

  if (insert_val != NULL) {
    IRB->CreateStore(insert_val, use_carved_obj);
  }

  IRB->CreateBr(end_block);
}

bool driver_pass::get_driver_func() {
  for (auto &F : Mod->functions()) {
    if (F.isIntrinsic() || !F.size()) {
      continue;
    }
    std::string func_name = F.getName().str();
    if (func_name == "LLVMFuzzerTestOneInput") {
      driver_func = &F;
      break;
    }
  }

  if (driver_func == NULL) {
    DEBUG0("Can't find LLVMFuzzerTestOneInput function\n");
    return false;
  }

  return true;
}

bool driver_pass::get_insert_val() {
  for (auto &GV : Mod->globals()) {
    if (GV.isDeclaration()) {
      continue;
    }
    std::string gv_name = GV.getName().str();
    if (gv_name == "use_carved_obj") {
      use_carved_obj = &GV;
      break;
    }
  }

  return (use_carved_obj != NULL);
}

static RegisterPass<driver_pass> X("driver", "Driver pass", false, false);

static void registerPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  auto p = new driver_pass();
  PM.add(p);
}

static RegisterStandardPasses RegisterPassOpt(
    PassManagerBuilder::EP_ModuleOptimizerEarly,
    [](const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
      PM.add(new driver_pass());
    });

// static RegisterStandardPasses RegisterPassO0(
//     PassManagerBuilder::EP_EnabledOnOptLevel0, registerPass);