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
#endif


using namespace llvm;

typedef llvm::iterator_range<llvm::Module::global_value_iterator> global_range;

namespace {

class driver_pass : public ModulePass {

 public:
  static char ID;
  driver_pass() : ModulePass(ID) { func_id = 0;}

  bool runOnModule(Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "driver instrumentation";

  }

 private:
  bool hookInstrs(Module &M);

  std::string target_name;
  Function * target_func;
  Function * main_func;
  void get_target_func();

  std::map<std::string, std::string> probe_link_names;
  void read_probe_list();
  std::string get_link_name(std::string);

  std::string find_param_name(Value * param, BasicBlock * BB);

  std::map<std::string, Constant *> new_string_globals;
  Constant * gen_new_string_constant(std::string name);
  
  DebugInfoFinder DbgFinder;
  Module * Mod;
  LLVMContext * Context;
  const DataLayout * DL;

  IRBuilder<> *IRB;
  Type        *VoidTy;
  IntegerType *Int8Ty;
  IntegerType *Int16Ty;
  IntegerType *Int32Ty;
  IntegerType *Int64Ty;
  IntegerType *Int128Ty;

  Type        *FloatTy;
  Type        *DoubleTy;
  
  PointerType *Int8PtrTy;
  PointerType *Int16PtrTy;
  PointerType *Int32PtrTy;
  PointerType *Int64PtrTy;
  PointerType *Int128PtrTy;
  PointerType *Int8PtrPtrTy;
  PointerType *Int8PtrPtrPtrTy;

  PointerType *FloatPtrTy;
  PointerType *DoublePtrTy;
  
  FunctionCallee __inputf_reader;


  int func_id;
};

}  // namespace

char driver_pass::ID = 0;

bool driver_pass::hookInstrs(Module &M) {
  LLVMContext &              C = M.getContext();
  const DataLayout & dataLayout = M.getDataLayout();
  Mod = &M;
  Context = &C;
  DL = &dataLayout;
  IRB = new IRBuilder<> (C);
  
  DbgFinder.processModule(M);

  VoidTy = Type::getVoidTy(C);
  Int8Ty = IntegerType::getInt8Ty(C);
  Int16Ty = IntegerType::getInt16Ty(C);
  Int32Ty = IntegerType::getInt32Ty(C);
  Int64Ty = IntegerType::getInt64Ty(C);
  Int128Ty = IntegerType::getInt128Ty(C);

  FloatTy = Type::getFloatTy(C);
  DoubleTy = Type::getDoubleTy(C);
  Int8PtrTy = PointerType::get(Int8Ty, 0);
  Int16PtrTy = PointerType::get(Int16Ty, 0);
  Int32PtrTy = PointerType::get(Int32Ty, 0);
  Int64PtrTy = PointerType::get(Int64Ty, 0);
  Int128PtrTy = PointerType::get(Int128Ty, 0);
  Int8PtrPtrTy = PointerType::get(Int8PtrTy, 0);
  Int8PtrPtrPtrTy = PointerType::get(Int8PtrPtrTy, 0);

  FloatPtrTy = Type::getFloatPtrTy(C);
  DoublePtrTy = Type::getDoublePtrTy(C);
  //Type        *DoubleTy = Type::getDoubleTy(C);

  __inputf_reader = M.getOrInsertFunction(get_link_name("__driver_inputf_reader"),
    VoidTy, Int8PtrPtrTy);

  get_target_func();

  DEBUG0("Iterating functions...\n");

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) { continue; }

    std::string func_name = F.getName().str();

    if (func_name == "main") {
      main_func = &F;
    } else if (target_func != &F) {
      F.deleteBody();
      //TODO : make stub
    }

    DEBUG0("done in " << func_name << "\n");
  }

  IRB->SetInsertPoint(main_func->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
  Value * argv = main_func->getArg(1);
  std::vector<Value *> reader_args {argv};
  IRB->CreateCall(__inputf_reader, reader_args);

  if (target_func == main_func) {
    //TODO
    
  } else {
    std::vector<Value *> target_args;
    IRB->CreateCall(target_func->getFunctionType(), target_func, target_args);
  }

  char * tmp = getenv("DUMP_IR");
  if (tmp) {
    M.dump();
  }

  delete IRB;
  return true;
}

bool driver_pass::runOnModule(Module &M) {

  DEBUG0("Running driver_pass\n");

  read_probe_list();
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

void driver_pass::read_probe_list() {
  std::string file_path = __FILE__;
  file_path = file_path.substr(0, file_path.rfind("/"));
  std::string probe_file_path
    = file_path.substr(0, file_path.rfind("/")) + "/lib/driver_probe_names.txt";
  
  std::ifstream list_file(probe_file_path);

  std::string line;
  while(std::getline(list_file, line)) {
    size_t space_loc = line.find(' ');
    probe_link_names.insert(std::make_pair(line.substr(0, space_loc)
      , line.substr(space_loc + 1)));
  }

  if (probe_link_names.size() == 0) {
    DEBUG0("Can't find lib/driver_probe_names.txt file!\n");
    std::abort();
  }
}

std::string driver_pass::get_link_name(std::string base_name) {
  auto search = probe_link_names.find(base_name);
  if (search == probe_link_names.end()) {
    DEBUG0("Can't find probe name : " << base_name << "! Abort.\n");
    std::abort();
  }

  return search->second;
}

Constant * driver_pass::gen_new_string_constant(std::string name) {

  auto search = new_string_globals.find(name);

  if (search == new_string_globals.end()) {
    Constant * new_global = IRB->CreateGlobalStringPtr(name);
    new_string_globals.insert(std::make_pair(name, new_global));
    return new_global;
  }

  return search->second;
}

std::string driver_pass::find_param_name(Value * param, BasicBlock * BB) {

  Instruction * ptr = NULL;

  for (auto instr_iter = BB->begin(); instr_iter != BB->end(); instr_iter++) {
    if ((ptr == NULL) && isa<StoreInst>(instr_iter)) {
      StoreInst * store_inst = dyn_cast<StoreInst>(instr_iter);
      if (store_inst->getOperand(0) == param) {
        ptr = (Instruction *) store_inst->getOperand(1);
      }
    } else if (isa<DbgVariableIntrinsic>(instr_iter)) {
      DbgVariableIntrinsic * intrinsic = dyn_cast<DbgVariableIntrinsic>(instr_iter);
      Value * valloc = intrinsic->getVariableLocationOp(0);

      if (valloc == ptr) {
        DILocalVariable * var = intrinsic->getVariable();
        return var->getName().str();
      }
    }
  }

  return "";
}

void driver_pass::get_target_func() {
  std::ifstream funcnames("funcs.txt");
  int func_idx = 0;
  char * func_idx_env = getenv("FUNCIDX");
  if (func_idx_env != NULL) {
    func_idx = atoi(func_idx_env);
  }

  std::string line;
  int line_idx = 0;
  while(std::getline(funcnames, line)) {
    if (line_idx == func_idx) {
      target_name = line;
      break;
    }
    line_idx ++;
  }

  for (auto &F : Mod->functions()) {
    if (F.isIntrinsic() || !F.size()) { continue; }
    std::string func_name = F.getName().str();    
    if (func_name == target_name) {
      target_func = &F;
      break;
    }
  }

  funcnames.close();
}

static void registerdriver_passPass(const PassManagerBuilder &,
                                           legacy::PassManagerBase &PM) {

  auto p = new driver_pass();
  PM.add(p);

}

static RegisterStandardPasses Registerdriver_passPass(
    PassManagerBuilder::EP_OptimizerLast, registerdriver_passPass);

static RegisterStandardPasses Registerdriver_passPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerdriver_passPass);

static RegisterStandardPasses Registerdriver_passPassLTO(
    PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
    registerdriver_passPass);

