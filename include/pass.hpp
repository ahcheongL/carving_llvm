#ifndef PASS_UTILS_HPP
#define PASS_UTILS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <set>
#include <string>
#include <map>
#include <fstream>
#include <vector>
#include <sys/time.h>
#include <sstream>

#include "llvm/Config/llvm-config.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"

//#define DEBUG0(x)
#ifndef DEBUG0
#define DEBUG0(x) (llvm::errs() << x)
#define DEBUGDUMP(x) (x->dump())
#else
#define DEBUGDUMP(x)
#endif

#define STRUCT_CARV_DEPTH 5

using namespace llvm;

std::string get_type_str(Type * type);
bool is_func_ptr_type(Type * type);

std::string get_link_name(std::string);
void read_probe_list(std::string);

Constant * gen_new_string_constant(std::string, IRBuilder<> *);
std::string find_param_name(Value *, BasicBlock *);

void get_struct_field_names_from_DIT(DIType *, std::vector<std::string> *);

extern int num_class_name_const;
extern std::vector<std::pair<Constant *, int>> class_name_consts;
extern std::map<StructType *, std::pair<int, Constant *>> class_name_map;
void get_class_type_info();

extern std::map<Function *, std::vector<GlobalVariable *>> global_var_uses;
void find_global_var_uses();

extern Type        *VoidTy;
extern IntegerType *Int1Ty;
extern IntegerType *Int8Ty;
extern IntegerType *Int16Ty;
extern IntegerType *Int32Ty;
extern IntegerType *Int64Ty;
extern IntegerType *Int128Ty;

extern Type        *FloatTy;
extern Type        *DoubleTy;

extern PointerType *Int8PtrTy;
extern PointerType *Int16PtrTy;
extern PointerType *Int32PtrTy;
extern PointerType *Int64PtrTy;
extern PointerType *Int128PtrTy;
extern PointerType *Int8PtrPtrTy;
extern PointerType *Int8PtrPtrPtrTy;

extern PointerType *FloatPtrTy;
extern PointerType *DoublePtrTy;

void get_llvm_types();

extern FunctionCallee mem_allocated_probe;
extern FunctionCallee remove_probe;
extern FunctionCallee record_func_ptr;
extern FunctionCallee argv_modifier;
extern FunctionCallee __carv_fini;
extern FunctionCallee strlen_callee;
extern FunctionCallee carv_char_func;
extern FunctionCallee carv_short_func;
extern FunctionCallee carv_int_func;
extern FunctionCallee carv_long_func;
extern FunctionCallee carv_longlong_func;
extern FunctionCallee carv_float_func;
extern FunctionCallee carv_double_func;
extern FunctionCallee carv_ptr_func;
extern FunctionCallee carv_ptr_name_update;
extern FunctionCallee struct_name_func;
extern FunctionCallee carv_name_push;
extern FunctionCallee carv_name_pop;
extern FunctionCallee carv_func_ptr;
extern FunctionCallee carv_func_call;
extern FunctionCallee carv_func_ret;
extern FunctionCallee update_carved_ptr_idx;
extern FunctionCallee keep_class_name;
extern FunctionCallee get_class_idx;
extern FunctionCallee get_class_size;
extern FunctionCallee class_carver;
extern FunctionCallee update_class_ptr;
extern FunctionCallee carv_time_begin;
extern FunctionCallee carv_time_end;

void get_carving_func_callees();

extern std::vector<AllocaInst *> tracking_allocas;
void Insert_alloca_probe(BasicBlock &);
bool Insert_mem_func_call_probe(Instruction *, std::string);
void Insert_carving_main_probe(BasicBlock &, Function &);

BasicBlock * insert_carve_probe(Value *, BasicBlock *);

extern std::set<std::string> struct_carvers;
void insert_struct_carve_probe(Value *, Type *);
void insert_struct_carve_probe_inner(Value *, Type *);

extern Module * Mod;
extern LLVMContext * Context;
extern const DataLayout * DL;
extern IRBuilder<> *IRB;
extern DebugInfoFinder DbgFinder;

void initialize_pass_contexts(Module &);

void construct_ditype_map();

extern unsigned int num_func_tracked;

BasicBlock * insert_gep_carve_probe(Value * gep_val, BasicBlock * cur_block);
BasicBlock * insert_array_carve_probe(Value * arr_ptr_val, BasicBlock * cur_block);

extern std::set<std::string> no_carve_funcs;
extern std::set<std::string> no_instrument_funcs;

void insert_check_carve_ready();

void insert_dealloc_probes();

extern Constant * const_carve_ready;

#endif