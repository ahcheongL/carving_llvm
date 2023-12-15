#ifndef CARVE_FUNC_ARGS_PASS_HPP
#define CARVE_FUNC_ARGS_PASS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "utils/pass.hpp"

using namespace std;

class CarverMPass : public llvm::ModulePass {
 public:
  static char ID;
  CarverMPass();

  bool runOnModule(llvm::Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  llvm::StringRef
#else
  const char *
#endif
  getPassName() const override;

 private:
  bool instrument_module();

  // Target function including main
  std::set<std::string> instrument_func_set;
  void get_instrument_func_set();

  void instrument_main(llvm::Function *F);

  void insert_global_carve_probe(llvm::Function *F);

  void instrument_func(llvm::Function *F);

  void insert_carve_probe_m(llvm::Value *val);

  void insert_gep_carve_probe_m(llvm::Value *gep_val);
  void insert_array_carve_probe_m(llvm::Value *val);
  void insert_struct_carve_probe_m(llvm::Value *val, llvm::Type *ty);
  void insert_struct_carve_probe_m_inner(llvm::Value *val, llvm::Type *ty);

  void gen_class_carver_m();

  void insert_alloca_probe(llvm::BasicBlock &);
  void insert_dealloc_probes();
  bool insert_mem_func_call_probe(llvm::Instruction *, std::string);
  Constant *get_mem_alloc_type(llvm::Instruction *call_inst);

  void insert_check_carve_ready();

  int func_id;

  std::set<std::string> struct_carvers_;

  llvm::FunctionCallee carv_file;
  llvm::FunctionCallee carv_open;
  llvm::FunctionCallee carv_close;

  vector<llvm::AllocaInst *> tracking_allocas;

  llvm::FunctionCallee mem_allocated_probe;
  llvm::FunctionCallee remove_probe;

  llvm::FunctionCallee record_func_ptr;

  llvm::FunctionCallee argv_modifier;
  llvm::FunctionCallee __carv_fini;
  llvm::FunctionCallee carv_char_func;
  llvm::FunctionCallee carv_short_func;
  llvm::FunctionCallee carv_int_func;
  llvm::FunctionCallee carv_long_func;
  llvm::FunctionCallee carv_longlong_func;
  llvm::FunctionCallee carv_float_func;
  llvm::FunctionCallee carv_double_func;
  llvm::FunctionCallee carv_ptr_func;
  llvm::FunctionCallee carv_func_ptr;
  llvm::FunctionCallee update_carved_ptr_idx;
  llvm::FunctionCallee keep_class_info;
  llvm::FunctionCallee class_carver;
  llvm::FunctionCallee record_func_ptr_index;

  llvm::FunctionCallee insert_ptr_end;
  llvm::FunctionCallee insert_ptr_idx;
  llvm::FunctionCallee insert_struct_begin;
  llvm::FunctionCallee insert_struct_end;

  llvm::FunctionCallee load_addr_probe;

  llvm::Constant *global_carve_ready;
  llvm::Constant *global_cur_class_idx;
  llvm::Constant *global_cur_class_size;

  llvm::FunctionCallee insert_obj_info;
};
#endif