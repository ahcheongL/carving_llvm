#include"pass.hpp"

namespace {

class extract_callseq_pass : public ModulePass {

 public:
  static char ID;
  extract_callseq_pass() : ModulePass(ID) { func_id = 0;}

  bool runOnModule(Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "extract info instrumentation";
  }

 private:
  bool hookInstrs(Module &M);
  
  DebugInfoFinder DbgFinder;
  Module * Mod;
  LLVMContext * Context;
  const DataLayout * DL;
  IRBuilder<> *IRB;

  std::set<std::string> complex_types;

  unsigned int get_num_fields(StructType * type);

  int func_id;
};

}  // namespace

char extract_callseq_pass::ID = 0;

bool extract_callseq_pass::hookInstrs(Module &M) {
  LLVMContext &              C = M.getContext();
  const DataLayout & dataLayout = M.getDataLayout();
  Mod = &M;
  Context = &C;
  DL = &dataLayout;
  IRB = new IRBuilder<> (C);
  
  DbgFinder.processModule(M);

  Type * VoidTy = Type::getVoidTy(C);
  Type * Int8Ty = Type::getInt8Ty(C);

  FunctionCallee open_call_seq = M.getOrInsertFunction("open_call_seq", VoidTy);
  FunctionCallee close_call_seq = M.getOrInsertFunction("close_call_seq", VoidTy);
  FunctionCallee write_call_seq = M.getOrInsertFunction("write_call_seq", VoidTy, Int8Ty);

  for (auto &F : M) {
    if (F.size() == 0) { continue; }
    std::string func_name = F.getName().str();

    if (func_name == "main") {
      IRB->SetInsertPoint(&F.getEntryBlock().front());
      IRB->CreateCall(open_call_seq, {});

      std::set<ReturnInst *> ret_instrs;
      for (auto &BB : F) {
        for (auto &I : BB) {
          if (isa<ReturnInst>(&I)) {
            ret_instrs.insert(dyn_cast<ReturnInst>(&I));
          }
        }
      }

      for (auto &ret_instr : ret_instrs) {
        IRB->SetInsertPoint(ret_instr);
        IRB->CreateCall(close_call_seq, {});
      }
    } else {
      IRB->SetInsertPoint(&F.getEntryBlock().front());
      Constant * name_const = gen_new_string_constant(func_name, IRB);
      IRB->CreateCall(write_call_seq, name_const);
    }
  }

  return true;
}

bool extract_callseq_pass::runOnModule(Module &M) {

  DEBUG0("Running extract_callseq_pass\n");

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