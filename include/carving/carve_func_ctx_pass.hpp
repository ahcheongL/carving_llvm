#ifndef CARVE_FUNC_CTX_PASS_HPP
#define CARVE_FUNC_CTX_PASS_HPP

#include "carve_pass.hpp"

class CarverFCPass : public ModulePass {
 public:
  static char ID;
  CarverFCPass() : ModulePass(ID) { func_id = 0; }

  bool runOnModule(Module &M);

#if LLVM_VERSION_MAJOR >= 4
  StringRef
#else
  const char *
#endif
  getPassName() const;

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

#endif