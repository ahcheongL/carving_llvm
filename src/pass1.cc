#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <set>
#include <string>
#include <map>
#include <fstream>
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


using namespace llvm;

typedef llvm::iterator_range<llvm::Module::global_value_iterator> global_range;

namespace {

class pass1 : public ModulePass {

 public:
  static char ID;
  pass1() : ModulePass(ID) { func_id = 0;}

  bool runOnModule(Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "carving instrumentation";

  }

 private:
  bool hookInstrs(Module &M);
  
  void read_probe_list();
  std::map<std::string, std::string> probe_link_names;
  std::string get_link_name(std::string);

  void Insert_alloca_probe(BasicBlock & entry_block, const DataLayout& DL);
  std::vector<AllocaInst *> tracking_allocas;
  void Insert_memfunc_probe(Instruction& IN, std::string callee_name);
  void Insert_main_probe(BasicBlock & entry_block, Function & F
    , global_range globals, const DataLayout & DL);

  std::map<std::string, Constant *> new_string_globals;
  Constant * gen_new_string_constant(std::string name);
  
  IRBuilder<> *IRB;
  Type        *VoidTy;
  IntegerType *Int8Ty;
  IntegerType *Int16Ty;
  IntegerType *Int32Ty;
  IntegerType *Int64Ty;
  IntegerType *Int128Ty;

  Type        *FloatTy;
  Type        *DoubleTy;
  
  PointerType *VoidPtrTy;
  PointerType *Int8PtrTy;
  PointerType *Int16PtrTy;
  PointerType *Int32PtrTy;
  PointerType *Int64PtrTy;
  PointerType *Int128PtrTy;
  PointerType *Int8PtrPtrTy;
  PointerType *Int8PtrPtrPtrTy;

  PointerType *FloatPtrTy;
  PointerType *DoublePtrTy;
  
  FunctionCallee mem_allocated_probe;
  FunctionCallee remove_probe;
  FunctionCallee __carv_init;
  FunctionCallee argv_modifier;
  FunctionCallee __carv_fini;
  FunctionCallee write_carved;
  FunctionCallee strlen_callee;
  FunctionCallee carv_char_func;
  FunctionCallee carv_short_func;
  FunctionCallee carv_int_func;
  FunctionCallee carv_long_func;
  FunctionCallee carv_longlong_func;
  FunctionCallee carv_float_func;
  FunctionCallee carv_double_func;

  int func_id;
};

}  // namespace

char pass1::ID = 0;

void pass1::Insert_alloca_probe(BasicBlock& entry_block, const DataLayout & DL) {
  std::vector<AllocaInst * > allocas;
  
  for (auto &IN : entry_block) {
    AllocaInst * tmp_instr;
    if ((tmp_instr = dyn_cast<AllocaInst>(&IN)) != NULL) {
      allocas.push_back(tmp_instr);
    } else if (allocas.size() != 0) {
      //We met not-alloca instruction
      IRB->SetInsertPoint(&IN);
      for (auto iter = allocas.begin(); iter != allocas.end(); iter++) {
        AllocaInst * alloc_instr = *iter;
        Type * allocated_type = alloc_instr->getAllocatedType();
        unsigned int size = DL.getTypeAllocSize(allocated_type);

        Value * casted_ptr = IRB->CreateCast(
          Instruction::CastOps::BitCast, alloc_instr, VoidPtrTy);
        Value * size_const = ConstantInt::get(Int32Ty, size);
        std::vector<Value *> args {casted_ptr, size_const};
        IRB->CreateCall(mem_allocated_probe, args);
        tracking_allocas.push_back(alloc_instr);
      }
      break;
    }
  }
}

void pass1::Insert_memfunc_probe(Instruction& IN, std::string callee_name) {
  IRB->SetInsertPoint(IN.getNextNonDebugInstruction());
  
  if (callee_name == "malloc") {
    //Track malloc
    std::vector<Value *> args {&IN, IN.getOperand(0)};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "realloc") {
    //Track realloc
    std::vector<Value *> args {&IN, IN.getOperand(1)};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "free") {
    //Track free
    std::vector<Value *> args {IN.getOperand(0)};
    IRB->CreateCall(remove_probe, args);
  } else if (callee_name == "llvm.memcpy.p0i8.p0i8.i64") {
    //Get some hint from memory related functions
    std::vector<Value *> args {IN.getOperand(0), IN.getOperand(2)};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "llvm.memmove.p0i8.p0i8.i64") {
    std::vector<Value *> args {IN.getOperand(0), IN.getOperand(2)};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strlen") {
    Value * add_one = IRB->CreateAdd(&IN, ConstantInt::get(Int64Ty, 1));
    std::vector<Value *> args {IN.getOperand(0), add_one};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strncpy") {
    std::vector<Value *> args {IN.getOperand(0), IN.getOperand(2)};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strcpy") {
    std::vector<Value *> strlen_args;
    strlen_args.push_back(IN.getOperand(0));
    Value * strlen_result = IRB->CreateCall(strlen_callee, strlen_args);
    Value * add_one = IRB->CreateAdd(strlen_result, ConstantInt::get(Int64Ty, 1));
    std::vector<Value *> args {IN.getOperand(0), add_one};
    IRB->CreateCall(mem_allocated_probe, args);
  }

  return;
}

void pass1::Insert_main_probe(BasicBlock & entry_block, Function & F
  , global_range globals, const DataLayout & DL) {

  IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());  

  Value * new_argc = NULL;
  Value * new_argv = NULL;

  if (F.arg_size() == 2) {
    Value * argc = F.getArg(0);
    Value * argv = F.getArg(1);
    AllocaInst * argc_ptr = IRB->CreateAlloca(Int32Ty);
    AllocaInst * argv_ptr = IRB->CreateAlloca(Int8PtrPtrTy);

    std::vector<Value *> argv_modifier_args;
    argv_modifier_args.push_back(argc_ptr);
    argv_modifier_args.push_back(argv_ptr);

    new_argc = IRB->CreateLoad(Int32Ty, argc_ptr);
    new_argv = IRB->CreateLoad(Int8PtrPtrTy, argv_ptr);

    argc->replaceAllUsesWith(new_argc);
    argv->replaceAllUsesWith(new_argv);

    IRB->SetInsertPoint((Instruction *) new_argc);

    IRB->CreateStore(argc, argc_ptr);
    IRB->CreateStore(argv, argv_ptr);

    IRB->CreateCall(argv_modifier, argv_modifier_args);

    Instruction * new_argv_load_instr = dyn_cast<Instruction>(new_argv);

    IRB->SetInsertPoint(new_argv_load_instr->getNextNonDebugInstruction());
  }  

  std::vector<Value *> args;
  IRB->CreateCall(__carv_init, args);

  //Global variables memory probing
  for (auto global_iter = globals.begin(); global_iter != globals.end(); global_iter++) {

    if (!isa<GlobalVariable>(*global_iter)) { continue; }
    
    // if ((*global_iter).getLinkage() == GlobalValue::LinkageTypes::InternalLinkage) {
    //   (*global_iter).setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
    // }
    
    Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, &(*global_iter), VoidPtrTy);
    Type * gv_type = (*global_iter).getValueType();
    unsigned int size = DL.getTypeAllocSize(gv_type);
    Value * size_const = ConstantInt::get(Int32Ty, size);
    std::vector<Value *> args{casted_ptr, size_const};
    IRB->CreateCall(mem_allocated_probe, args);
  }

  if (F.arg_size() == 2) {
    Constant * argc_name_const = gen_new_string_constant("param1");
    std::vector<Value *> probe_args1 {new_argc, argc_name_const};
    IRB->CreateCall(carv_int_func, probe_args1);

    //TODO : argv

    Constant * func_name_const = gen_new_string_constant("main");
    Constant * func_id_const = ConstantInt::get(Int32Ty, func_id++);

    std::vector<Value *> probe_args {func_name_const, func_id_const};
    IRB->CreateCall(write_carved, probe_args);
  }
  return;
}

bool pass1::hookInstrs(Module &M) {
  LLVMContext &              C = M.getContext();
  const DataLayout & dataLayout = M.getDataLayout();
  IRB = new IRBuilder<> (C);

  VoidTy = Type::getVoidTy(C);
  Int8Ty = IntegerType::getInt8Ty(C);
  Int16Ty = IntegerType::getInt16Ty(C);
  Int32Ty = IntegerType::getInt32Ty(C);
  Int64Ty = IntegerType::getInt64Ty(C);
  Int128Ty = IntegerType::getInt128Ty(C);

  FloatTy = Type::getFloatTy(C);
  DoubleTy = Type::getDoubleTy(C);

  VoidPtrTy = PointerType::get(VoidTy, 0);
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
  
  mem_allocated_probe = M.getOrInsertFunction(
    get_link_name("__mem_allocated_probe"), VoidTy, VoidPtrTy, Int32Ty);
  remove_probe = M.getOrInsertFunction(get_link_name("__remove_mem_allocated_probe")
    , VoidTy, VoidPtrTy);
  __carv_init = M.getOrInsertFunction(get_link_name("__carv_init")
    , VoidTy, VoidTy);
  argv_modifier = M.getOrInsertFunction(get_link_name("__argv_modifier")
    , VoidTy, Int32PtrTy, Int8PtrPtrPtrTy);
  write_carved = M.getOrInsertFunction(get_link_name("Write_carved")
    , VoidTy, Int8PtrTy, Int32Ty);
  __carv_fini = M.getOrInsertFunction(get_link_name("__carv_FINI")
    , VoidTy, VoidTy);
  strlen_callee = M.getOrInsertFunction("strlen", Int64Ty, Int8PtrTy);
  carv_char_func = M.getOrInsertFunction(get_link_name("Carv_char")
    , VoidTy, Int8PtrTy, Int8PtrTy);
  carv_short_func = M.getOrInsertFunction(get_link_name("Carv_short")
    , VoidTy, Int16PtrTy, Int8PtrTy);
  carv_int_func = M.getOrInsertFunction(get_link_name("Carv_int")
    , VoidTy, Int32PtrTy, Int8PtrTy);
  carv_long_func = M.getOrInsertFunction(get_link_name("Carv_long")
    , VoidTy, Int64PtrTy, Int8PtrTy);
  carv_longlong_func = M.getOrInsertFunction(get_link_name("Carv_longlong")
    , VoidTy, Int8PtrTy, Int8PtrTy);
  carv_float_func = M.getOrInsertFunction(get_link_name("Carv_float")
    , VoidTy, FloatPtrTy, Int8PtrTy);
  carv_double_func = M.getOrInsertFunction(get_link_name("Carv_double")
    , VoidTy, DoublePtrTy, Int8PtrTy);

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) { continue; }

    std::string func_name = F.getName().str();
    if (func_name.find("__CROWN") != std::string::npos) { continue; }
    if (func_name.find("Carv_") != std::string::npos) { continue; }

    BasicBlock& entry_block = F.getEntryBlock();
    Insert_alloca_probe(entry_block, dataLayout);

    //Main argc argv handling
    if (func_name == "main") {
      Insert_main_probe(entry_block, F, M.global_values(), dataLayout);
    } else {
      int param_idx = 0;
      for (auto arg_iter = F.arg_begin(); arg_iter != F.arg_end(); arg_iter++) {
        std::string param_name = "parm_" + std::to_string(param_idx++);
        IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());
        Value * func_arg = &(*arg_iter);
        Type * arg_type = func_arg->getType();
        Constant * param_name_const = gen_new_string_constant(param_name);
        
        std::vector<Value *> probe_args {func_arg, param_name_const};
        if (arg_type == Int8Ty) {
          IRB->CreateCall(carv_char_func, probe_args);
        } else if (arg_type == Int16Ty) {
          IRB->CreateCall(carv_short_func, probe_args);
        } else if (arg_type == Int32Ty) {
          IRB->CreateCall(carv_int_func, probe_args);
        } else if (arg_type == Int64Ty) {
          IRB->CreateCall(carv_long_func, probe_args);
        } else if (arg_type == Int128Ty) {
          IRB->CreateCall(carv_longlong_func, probe_args);
        } else if (arg_type == FloatTy) {
          IRB->CreateCall(carv_float_func, probe_args);
        } else if (arg_type == DoubleTy) {
          IRB->CreateCall(carv_double_func, probe_args);
        }
      }

      Constant * func_name_const = gen_new_string_constant(func_name);
      Constant * func_id_const = ConstantInt::get(Int32Ty, func_id++);

      std::vector<Value *> probe_args {func_name_const, func_id_const};
      IRB->CreateCall(write_carved, probe_args);
    }

    //Memory tracking probes
    for (auto &BB : F) {
      for (auto &IN : BB) {
        CallInst * call_instr;
        if ((call_instr = dyn_cast<CallInst>(&IN)) != NULL) {
          Function * callee = call_instr->getCalledFunction();
          if (callee == NULL) { continue; }
          std::string callee_name = callee->getName().str();
          Insert_memfunc_probe(IN, callee_name);
        } else if (isa<ReturnInst>(&IN)) {
          
          //Remove alloca (local variable) memory tracking info.
          IRB->SetInsertPoint(&IN);
          for (auto iter = tracking_allocas.begin(); iter != tracking_allocas.end(); iter++) {
            AllocaInst * alloc_instr = *iter;

            Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, alloc_instr, VoidPtrTy);
            std::vector<Value *> args {casted_ptr};
            IRB->CreateCall(remove_probe, args);
          }

          //Insert fini
          if (func_name == "main") {
            IRB->CreateCall(__carv_fini, std::vector<Value *>());
          }
        }
      }
    }

    tracking_allocas.clear();
  }

  char * tmp = getenv("DUMP_IR");
  if (tmp) {
    M.dump();
  }

  delete IRB;
  return true;
}

bool pass1::runOnModule(Module &M) {

  llvm::errs() << "Running pass1\n";

  read_probe_list();
  hookInstrs(M);
  verifyModule(M);

  return true;
}

void pass1::read_probe_list() {
  std::string file_path = __FILE__;
  file_path = file_path.substr(0, file_path.rfind("/"));
  std::string probe_file_path
    = file_path.substr(0, file_path.rfind("/")) + "/lib/probe_names.txt";
  
  std::ifstream list_file(probe_file_path);

  std::string line;
  while(std::getline(list_file, line)) {
    size_t space_loc = line.find(' ');
    probe_link_names.insert(std::make_pair(line.substr(0, space_loc)
      , line.substr(space_loc + 1)));
  }

  if (probe_link_names.size() == 0) {
    llvm::errs() << "Can't find lib/probe_names.txt file!\n";
    std::abort();
  }
}

std::string pass1::get_link_name(std::string base_name) {
  auto search = probe_link_names.find(base_name);
  if (search == probe_link_names.end()) {
    llvm::errs() << "Can't find probe name : " << base_name << "! Abort.\n";
    std::abort();
  }

  return search->second;
}

Constant * pass1::gen_new_string_constant(std::string name) {

  auto search = new_string_globals.find(name);

  if (search == new_string_globals.end()) {
    Constant * new_global = IRB->CreateGlobalStringPtr(name);
    new_string_globals.insert(std::make_pair(name, new_global));
    return new_global;
  }

  return search->second;
}

static void registerpass1Pass(const PassManagerBuilder &,
                                           legacy::PassManagerBase &PM) {

  auto p = new pass1();
  PM.add(p);

}

static RegisterStandardPasses Registerpass1Pass(
    PassManagerBuilder::EP_OptimizerLast, registerpass1Pass);

static RegisterStandardPasses Registerpass1Pass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerpass1Pass);

static RegisterStandardPasses Registerpass1PassLTO(
    PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
    registerpass1Pass);

