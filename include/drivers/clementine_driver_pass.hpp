#ifndef CL_DRIVER_PASS_HPP
#define CL_DRIVER_PASS_HPP

#include "drivers/driver_pass.hpp"

class ClementinePass : public ModulePass {
 public:
  static char ID;
  ClementinePass();

  bool runOnModule(Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  StringRef
#else
  const char *
#endif
  getPassName() const override;

 private:
  bool instrument_module();

  std::vector<Function *> func_list;

  // To measure coverage
  std::map<std::string, std::map<std::string, std::set<std::string>>>
      file_bb_map;

  void instrument_main_func(Function *main_func);
  void instrument_load_class_func(Function *);
  void instrument_load_default_func(Function *);
  bool is_load_class_func(Function *);
  bool is_load_default_func(Function *);

  std::set<std::string> default_struct_replayes;
  void insert_default_struct_replay_probe_inner(Value *, Type *);
  void insert_default_gep_replay_probe(Value *);

  void insert_default_struct_replay_probe(Value *, Type *);
  void insert_default_replay_probe(Type *, Value *);

  void gen_default_class_replay();

  //   void cl_select_default_file(int exec_idx, int ctx_idx) {
  //     __driver_select_default_file(exec_idx, ctx_idx);
  //   }
  void instrument_select_file_func(Function *);
  void instrument_cl_test_driver(Function *);

  void instrument_bb_cov(Function *, const std::string &, const std::string &);

  void get_target_func_idx();

  void instrument_stub(Function *);

  std::map<Function *, int> target_func_idx_map;
};

#endif