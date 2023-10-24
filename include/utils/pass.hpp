#ifndef PASS_UTILS_HPP
#define PASS_UTILS_HPP

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

#ifdef DEBUG
#define DEBUG0(x) (llvm::errs() << x)
#define DEBUGDUMP(x) (x->dump())
#else
#define DEBUG0(x)
#define DEBUGDUMP(x)
#endif

#define STRUCT_CARV_DEPTH 100

using namespace llvm;

std::string get_type_str(Type *type);
bool is_func_ptr_type(Type *type);

std::string get_link_name(std::string);

Constant *gen_new_string_constant(std::string, IRBuilder<> *);
std::string find_param_name(Value *, BasicBlock *);

void get_struct_field_names_from_DIT(DIType *, std::vector<std::string> *);

extern int num_class_name_const;
extern std::vector<std::pair<Constant *, int>> class_name_consts;
extern std::map<StructType *, std::pair<int, Constant *>> class_name_map;
void get_class_type_info();

extern std::map<Function *, std::vector<GlobalVariable *>> global_var_uses;
void find_global_var_uses();

extern Type *VoidTy;
extern IntegerType *Int1Ty;
extern IntegerType *Int8Ty;
extern IntegerType *Int16Ty;
extern IntegerType *Int32Ty;
extern IntegerType *Int64Ty;
extern IntegerType *Int128Ty;

extern Type *FloatTy;
extern Type *DoubleTy;

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

extern Module *Mod;
extern LLVMContext *Context;
extern const DataLayout *DL;
extern IRBuilder<> *IRB;
extern DebugInfoFinder DbgFinder;

void initialize_pass_contexts(Module &);

void construct_ditype_map();

extern std::set<Function *> forbid_func_set;
bool is_inst_forbid_func(Function *);

void check_and_dump_module();
std::string convert_symbol_str(std::string);
#endif