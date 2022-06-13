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

using namespace llvm;

typedef llvm::iterator_range<llvm::Module::global_value_iterator> global_range;

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
void get_class_type_info(Module *, IRBuilder<>*, const DataLayout *);

extern std::vector<Value *> empty_args;

#endif