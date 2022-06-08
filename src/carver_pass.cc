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

class carver_pass : public ModulePass {

 public:
  static char ID;
  carver_pass() : ModulePass(ID) { func_id = 0;}

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

  std::set<std::string> instrument_func_set;
  void get_instrument_func_set();

  std::map<Function *, std::set<Constant *>> global_var_uses;
  void find_global_var_uses();

  void Insert_alloca_probe(BasicBlock & entry_block);
  std::vector<AllocaInst *> tracking_allocas;
  void Insert_callinst_probe(Instruction *, std::string, bool);
  void Insert_main_probe(BasicBlock & entry_block, Function & F
    , global_range globals);
  BasicBlock * insert_carve_probe(Value * val, BasicBlock * BB);

  std::set<std::string> struct_carvers;
  void insert_struct_carve_probe(Value * struct_ptr, Type * struct_type);
  void insert_struct_carve_probe_inner(Value * struct_ptr, Type * struct_type);
  
  void insert_global_carve_probe(Function * F, BasicBlock * BB);

  std::string find_param_name(Value * param, BasicBlock * BB);

  std::map<std::string, Constant *> new_string_globals;
  Constant * gen_new_string_constant(std::string name);

  int num_class_name_const = 0;
  std::vector<Constant *> class_name_consts;
  std::map<Type *, std::pair<int, Constant *>> class_name_map;
  void add_class_string_const(Type *);
  std::map<Type *, std::set<Type *>> derived_class_types;
  void get_class_type_info();

  bool is_func_ptr_type(Type *);
  
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
  
  FunctionCallee mem_allocated_probe;
  FunctionCallee remove_probe;
  FunctionCallee record_func_ptr;
  FunctionCallee argv_modifier;
  FunctionCallee __carv_fini;
  FunctionCallee strlen_callee;
  FunctionCallee carv_char_func;
  FunctionCallee carv_short_func;
  FunctionCallee carv_int_func;
  FunctionCallee carv_long_func;
  FunctionCallee carv_longlong_func;
  FunctionCallee carv_float_func;
  FunctionCallee carv_double_func;
  FunctionCallee carv_ptr_func;
  FunctionCallee carv_ptr_name_update;
  FunctionCallee struct_name_func;
  FunctionCallee carv_name_push;
  FunctionCallee carv_name_pop;
  FunctionCallee carv_func_ptr;
  FunctionCallee carv_func_call;
  FunctionCallee carv_func_ret;
  FunctionCallee update_carved_ptr_idx;
  FunctionCallee mem_alloc_type;
  FunctionCallee name_free_pop;
  FunctionCallee keep_class_name;
  FunctionCallee get_class_name_idx;
  FunctionCallee pop_carving_obj;

  int func_id;
};

}  // namespace

char carver_pass::ID = 0;

bool carver_pass::is_func_ptr_type(Type * type) {
  if (type->isPointerTy()) {
    PointerType * ptrtype = dyn_cast<PointerType>(type);
    Type * elem_type = ptrtype->getPointerElementType();
    return elem_type->isFunctionTy();
  }
  return false;
}

void carver_pass::add_class_string_const(Type * class_type) {
  std::string typestr;
  raw_string_ostream typestr_stream(typestr);
  class_type->print(typestr_stream);

  Constant * typename_const = gen_new_string_constant(typestr);
  
  class_name_consts.push_back(typename_const);
  class_name_map.insert(
    std::make_pair(class_type
      , std::make_pair(num_class_name_const++, typename_const)));  
}

void carver_pass::get_class_type_info() {

  //Set dummy location...
  for (auto &F : Mod->functions()) {
    std::string func_name = F.getName().str();
    if (func_name == "main") {
      IRB->SetInsertPoint(F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
      break;
    }
  }

  auto structs = Mod->getIdentifiedStructTypes();

  for (auto iter : structs) {
    if (iter->getName().contains("class")) {
      for (auto iter2 : iter->elements()) {
        if ((iter2->isStructTy()) && (iter2->getStructName().contains("class"))) {
          auto search = derived_class_types.find(iter2);
          if (search == derived_class_types.end()) {
            derived_class_types.insert(std::make_pair(iter2, std::set<Type*>()));
          }
          derived_class_types[iter2].insert(iter);
        }
      }
    }
  }

  //Get derived type recursively
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto iter1 : derived_class_types) {
      for (auto derived_type : iter1.second) {
        auto search = derived_class_types.find(derived_type);
        if (search != derived_class_types.end()) {
          for (auto derived_derived_type : search->second) {
            if (derived_class_types[iter1.first].insert(derived_derived_type).second) {
              changed = true;
              break;
            }
          }
        }
        if (changed) break;
      }
      if (changed) break;
    }
  }

  for (auto iter : derived_class_types) {
    auto search = class_name_map.find(iter.first);
    if (search == class_name_map.end()) {
      add_class_string_const(iter.first);
    }
    for (auto iter2 : iter.second) {
      auto search = class_name_map.find(iter2);
      if (search == class_name_map.end()) {
        add_class_string_const(iter2);
      }
    }
  }
}

void carver_pass::find_global_var_uses() {
  for (auto &F : Mod->functions()) {
    for (auto &BB : F.getBasicBlockList()) {
      for (auto &Instr : BB.getInstList()) {
        for (auto &op : Instr.operands()) {
          if (isa<Constant>(op)) {
            Constant * constant = dyn_cast<Constant>(op);
            if (isa<Function>(constant)) { continue; }
            if (constant->getName().str().size() == 0) { continue; }

            auto search = global_var_uses.find(&F);
            if (search == global_var_uses.end()) {
              global_var_uses.insert(std::make_pair(&F, std::set<Constant *>()));
            }
            global_var_uses[&F].insert(constant);
          }
        }
      }
    }
  }

  //TODO : track callee's uses
  

  /*
  for (auto iter : global_var_uses) {
    DEBUG0(iter.first->getName().str() << " : \n");
    for (auto iter2 : iter.second) {
      DEBUG0(iter2->getName().str() << "\n");
      iter2->dump();
    }
  }
  */
  
}

void carver_pass::Insert_alloca_probe(BasicBlock& entry_block) {
  std::vector<AllocaInst * > allocas;
  
  for (auto &IN : entry_block) {
    AllocaInst * tmp_instr;
    if ((tmp_instr = dyn_cast<AllocaInst>(&IN)) != NULL) {
      allocas.push_back(tmp_instr);
    } else if (allocas.size() != 0) { //We met non-alloca instruction
      
      IRB->SetInsertPoint(&IN);
      for (auto iter = allocas.begin(); iter != allocas.end(); iter++) {
        AllocaInst * alloc_instr = *iter;
        Type * allocated_type = alloc_instr->getAllocatedType();
        Type * alloc_instr_type = alloc_instr->getType();
        unsigned int size = DL->getTypeAllocSize(allocated_type);

        Value * casted_ptr = alloc_instr;
        if (alloc_instr_type != Int8PtrTy) {
          casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast
            , alloc_instr, Int8PtrTy);
        }

        if (allocated_type->isStructTy()) {
          std::string typestr;
          raw_string_ostream typestr_stream(typestr);
          allocated_type->print(typestr_stream);
          Constant * typename_const = gen_new_string_constant(typestr);
          std::vector<Value *> args1 {casted_ptr, typename_const};
          IRB->CreateCall(mem_alloc_type, args1);
        }

        Value * size_const = ConstantInt::get(Int32Ty, size);
        std::vector<Value *> args {casted_ptr, size_const};
        IRB->CreateCall(mem_allocated_probe, args);
        tracking_allocas.push_back(alloc_instr);
      }
      break;
    }
  }
}

void carver_pass::Insert_callinst_probe(Instruction * IN
  , std::string callee_name, bool insert_ret_probe) {
  IRB->SetInsertPoint(IN->getNextNonDebugInstruction());
  
  if (callee_name == "malloc") {
    //Track malloc
    Value * size = IN->getOperand(0);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {IN, size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "realloc") {
    //Track realloc
    std::vector<Value *> args0 {IN->getOperand(0)};
    IRB->CreateCall(remove_probe, args0);
    std::vector<Value *> args1 {IN, IN->getOperand(1)};
    IRB->CreateCall(mem_allocated_probe, args1);
  } else if (callee_name == "free") {
    //Track free
    std::vector<Value *> args {IN->getOperand(0)};
    IRB->CreateCall(remove_probe, args);
  } else if (callee_name == "llvm.memcpy.p0i8.p0i8.i64") {
    //Get some hint from memory related functions
    // Value * size = IN.getOperand(2);
    // if (size->getType() == Int64Ty) {
    //   size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    // }
    // std::vector<Value *> args {IN.getOperand(0), size};
    // IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "llvm.memmove.p0i8.p0i8.i64") {
    // Value * size = IN.getOperand(2);
    // if (size->getType() == Int64Ty) {
    //   size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    // }
    // std::vector<Value *> args {IN.getOperand(0), size};
    // IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strlen") {
    // Value * add_one = IRB->CreateAdd(&IN, ConstantInt::get(Int64Ty, 1));
    // Value * size = IRB->CreateCast(Instruction::CastOps::Trunc, add_one, Int32Ty);
    // std::vector<Value *> args {IN.getOperand(0), size};
    // IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strncpy") {
    // Value * size = IN.getOperand(2);
    // if (size->getType() == Int64Ty) {
    //   size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    // }
    // std::vector<Value *> args {IN.getOperand(0), size};
    // IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strcpy") {
    // std::vector<Value *> strlen_args;
    // strlen_args.push_back(IN.getOperand(0));
    // Value * strlen_result = IRB->CreateCall(strlen_callee, strlen_args);
    // Value * add_one = IRB->CreateAdd(strlen_result, ConstantInt::get(Int64Ty, 1));
    // std::vector<Value *> args {IN.getOperand(0), add_one};
    // IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "_Znwm") {
    //new operator
    CastInst * cast_instr;
    if ((cast_instr = dyn_cast<CastInst>(IN->getNextNonDebugInstruction()))) {
      Type * cast_type = cast_instr->getType();
      std::string typestr;
      raw_string_ostream typestr_stream(typestr);
      cast_type->print(typestr_stream);
      Constant * typename_const = gen_new_string_constant(typestr);
      std::vector<Value *> args1 {IN, typename_const};
      IRB->CreateCall(mem_alloc_type, args1);
    }

    Value * size = IN->getOperand(0);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {IN, size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "_ZdlPv") {
    //delete operator
    std::vector<Value *> args {IN->getOperand(0)};
    IRB->CreateCall(remove_probe, args);
  } else if (insert_ret_probe) {
    if (IN->getType() != VoidTy) {
      Constant * name_const = gen_new_string_constant(callee_name + "_ret");
      std::vector<Value *> push_args {name_const};
      IRB->CreateCall(carv_name_push, push_args);

      insert_carve_probe(IN, IN->getParent());

      std::vector<Value *> pop_args;
      IRB->CreateCall(carv_name_pop, pop_args);
    }
  }

  return;
}

void carver_pass::Insert_main_probe(BasicBlock & entry_block, Function & F
  , global_range globals) {

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
  } else {
    llvm::errs() << "This pass requires a main function"
      << " which has argc, argv arguments\n";
    std::abort();
  }

  //Global variables memory probing
  for (auto global_iter = globals.begin(); global_iter != globals.end(); global_iter++) {

    if (!isa<GlobalVariable>(*global_iter)
      && !isa<Function>(*global_iter)) { continue; }

    if (isa<Function>(*global_iter)) {
      Function * global_f = dyn_cast<Function>(&(*global_iter));
      if (global_f->size() == 0) { continue; }
    } else if (isa<GlobalVariable>(*global_iter)) {
      GlobalVariable * global_v = dyn_cast<GlobalVariable>(&(*global_iter));
      if (global_v->getName().str().find("llvm.") != std::string::npos) { continue; }
    }
    
    Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, &(*global_iter), Int8PtrTy);
    Type * gv_type = (*global_iter).getValueType();
    unsigned int size = 8;
    if (isa<GlobalVariable>(*global_iter)) {
      size = DL->getTypeAllocSize(gv_type);
    }
    Value * size_const = ConstantInt::get(Int32Ty, size);
    std::vector<Value *> args{casted_ptr, size_const};
    IRB->CreateCall(mem_allocated_probe, args);
  }

  //Record func ptr
  for (auto &Func : Mod->functions()) {
    if (Func.size() == 0) { continue; }
    Constant * func_name_const = gen_new_string_constant(Func.getName().str());
    Value * cast_val = IRB->CreateCast(Instruction::CastOps::BitCast
      , (Value *) &Func, Int8PtrTy);
    std::vector<Value *> probe_args {cast_val, func_name_const};
    IRB->CreateCall(record_func_ptr, probe_args);
  }

  //Record class type string constants
  for (auto iter : class_name_consts) {
    std::vector<Value *> args {iter};
    IRB->CreateCall(keep_class_name, args);
  }

  return;
}

void carver_pass::insert_global_carve_probe(Function * F, BasicBlock * BB) {

  BasicBlock * cur_block = BB;

  auto search = global_var_uses.find(F);
  if (search != global_var_uses.end()) {
    for (auto glob_iter : search->second) {
      std::string glob_name = glob_iter->getName().str();

      Type * const_type = glob_iter->getType();
      assert(const_type->isPointerTy());
      Type * pointee_type = dyn_cast<PointerType>(const_type)->getPointerElementType();

      Constant * glob_name_const = gen_new_string_constant(glob_name);
      std::vector<Value *> push_args {glob_name_const};
      IRB->CreateCall(carv_name_push, push_args);

      if (pointee_type->isStructTy()) {
        insert_struct_carve_probe((Value *) glob_iter, pointee_type);
        std::vector<Value *> pop_args;
        IRB->CreateCall(name_free_pop, pop_args);
      } else {
        Value * load_val = IRB->CreateLoad(pointee_type, (Value *) glob_iter);
        cur_block = insert_carve_probe(load_val, cur_block);
        std::vector<Value *> pop_args;
        IRB->CreateCall(carv_name_pop, pop_args);
      }

      
    }
  }

  return;
}

BasicBlock * carver_pass::insert_carve_probe(Value * val, BasicBlock * BB) {
  Type * val_type = val->getType();

  std::vector<Value *> probe_args {val};
  if (val_type == Int8Ty) {
    IRB->CreateCall(carv_char_func, probe_args);
  } else if (val_type == Int16Ty) {
    IRB->CreateCall(carv_short_func, probe_args);
  } else if (val_type == Int32Ty) {
    IRB->CreateCall(carv_int_func, probe_args);
  } else if (val_type == Int64Ty) {
    IRB->CreateCall(carv_long_func, probe_args);
  } else if (val_type == Int128Ty) {
    IRB->CreateCall(carv_longlong_func, probe_args);
  } else if (val_type == FloatTy) {
    IRB->CreateCall(carv_float_func, probe_args);
  } else if (val_type == DoubleTy) {
    IRB->CreateCall(carv_double_func, probe_args);
  } else if (val_type->isFunctionTy() || val_type->isArrayTy()) {
    //Is it possible to reach here?
  } else if (val_type->isPointerTy()) {
    PointerType * ptrtype = dyn_cast<PointerType>(val_type);
    //type that we don't know.
    if (ptrtype->isOpaque() || ptrtype->isOpaquePointerTy()) { return BB; }
    Type * pointee_type = ptrtype->getPointerElementType();

    if (isa<StructType> (pointee_type)) {
      StructType * tmptype = dyn_cast<StructType>(pointee_type);
      if (tmptype->isOpaque()) { return BB; }
    }

    unsigned pointee_size = DL->getTypeAllocSize(pointee_type);
    if (pointee_size == 0) { return BB; }

    unsigned array_size = pointee_size;

    bool is_array_pointee = pointee_type->isArrayTy();
    if (is_array_pointee) {
      ArrayType * arrtype = dyn_cast<ArrayType>(pointee_type);
      Type * array_elem_type = arrtype->getArrayElementType();
      val_type = PointerType::get(array_elem_type, 0);
      pointee_type = array_elem_type;
      val = IRB->CreateCast(Instruction::CastOps::BitCast, val, val_type);
      pointee_size = DL->getTypeAllocSize(pointee_type);
    }

    //get 0 initialized index
    Instruction * index_alloc = IRB->CreateAlloca(Int32Ty);
    Value * index_store = IRB->CreateStore(ConstantInt::get(Int32Ty, 0), index_alloc);
    Value * index_load = IRB->CreateLoad(Int32Ty, index_alloc);
    Instruction * last_instr = (Instruction*) index_load;

    Value * ptrval = val;
    if (val_type != Int8PtrTy) {
      ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, val, Int8PtrTy);
      last_instr = (Instruction *) ptrval;
    }

    Value * pointer_size;
    if (is_array_pointee) {
      pointer_size = (Instruction *) IRB->CreateSDiv(
        ConstantInt::get(Int32Ty, array_size)
        , ConstantInt::get(Int32Ty, pointee_size));
    } else {
      //Call Carv_pointer
      std::vector<Value *> probe_args {ptrval};
      Value * end_size = IRB->CreateCall(carv_ptr_func, probe_args);

      pointer_size = (Instruction *)
        IRB->CreateSDiv(end_size, ConstantInt::get(Int32Ty, pointee_size));
      last_instr = (Instruction *) pointer_size;
    }

    //Make loop block
    BasicBlock * loopblock = BB->splitBasicBlock(last_instr->getNextNonDebugInstruction());
    BasicBlock * const loopblock_start = loopblock;

    IRB->SetInsertPoint(last_instr->getNextNonDebugInstruction());
    
    Instruction * cmp_instr1 = (Instruction *)
      IRB->CreateICmpEQ(pointer_size, ConstantInt::get(Int32Ty, 0));
    if (!is_array_pointee) {
      last_instr = cmp_instr1;
    }

    IRB->SetInsertPoint(loopblock->getFirstNonPHIOrDbgOrLifetime());
    PHINode * index_phi = IRB->CreatePHI(Int32Ty, 2);
    index_phi->addIncoming(index_load, BB);

    Value * getelem_instr = IRB->CreateGEP(pointee_type, val, index_phi);

    std::vector<Value *> probe_args2 {index_phi};
    IRB->CreateCall(carv_ptr_name_update, probe_args2);

    if (pointee_type->isStructTy()) {
      insert_struct_carve_probe(getelem_instr, pointee_type);
      std::vector<Value *> empty_args {};
      IRB->CreateCall(name_free_pop, empty_args);
    } else if (is_func_ptr_type(pointee_type)) {
      Value * ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, val, Int8PtrTy);
      std::vector<Value *> probe_args {ptrval};
      IRB->CreateCall(carv_func_ptr, probe_args);
      std::vector<Value *> empty_args {};
      IRB->CreateCall(carv_name_pop, empty_args);
    } else {
      Value * load_ptr = IRB->CreateLoad(pointee_type, getelem_instr);
      loopblock = insert_carve_probe(load_ptr, loopblock);
      std::vector<Value *> empty_args {};
      IRB->CreateCall(carv_name_pop, empty_args);
    }

    Value * index_update_instr
      = IRB->CreateAdd(index_phi, ConstantInt::get(Int32Ty, 1));
    index_phi->addIncoming(index_update_instr, loopblock);

    Instruction * cmp_instr2
      = (Instruction *) IRB->CreateICmpSLT(index_update_instr, pointer_size);

    BasicBlock * endblock
      = loopblock->splitBasicBlock(cmp_instr2->getNextNonDebugInstruction());

    IRB->SetInsertPoint(last_instr->getNextNonDebugInstruction());

    Instruction * BB_term
      = IRB->CreateCondBr(cmp_instr1, endblock, loopblock_start);
    BB_term->removeFromParent();
    
    //remove old terminator
    Instruction * old_term = BB->getTerminator();
    ReplaceInstWithInst(old_term, BB_term);

    IRB->SetInsertPoint(cmp_instr2->getNextNonDebugInstruction());

    Instruction * loopblock_term
      = IRB->CreateCondBr(cmp_instr2, loopblock_start, endblock);

    loopblock_term->removeFromParent();

    //remove old terminator
    old_term = loopblock->getTerminator();
    ReplaceInstWithInst(old_term, loopblock_term);

    IRB->SetInsertPoint(endblock->getFirstNonPHIOrDbgOrLifetime());

    return endblock;
  }

  return BB;
}

static void get_elem_names(DIType * dit, std::vector<std::string> * elem_names) {
  while ((dit != NULL) && (
    isa<DIDerivedType>(dit) || isa<DISubroutineType>(dit))) {
    if (isa<DIDerivedType>(dit)) {
      DIDerivedType * tmptype = dyn_cast<DIDerivedType>(dit);
      dit = tmptype->getBaseType();
    } else {
      DISubroutineType * tmptype = dyn_cast<DISubroutineType>(dit);
      //TODO
      dit = NULL;
      break;
    }
  }

  if ((dit == NULL) || (!isa<DICompositeType>(dit))) {
    return;
  }

  DICompositeType * struct_DIT = dyn_cast<DICompositeType>(dit);
  int field_idx = 0;
  for (auto iter2 : struct_DIT->getElements()) {
    if (isa<DIDerivedType>(iter2)) {
      DIDerivedType * elem_DIT = dyn_cast<DIDerivedType>(iter2);
      dwarf::Tag elem_tag = elem_DIT->getTag();
      std::string elem_name = "";
      if (elem_tag == dwarf::Tag::DW_TAG_member) {
        elem_name = elem_DIT->getName().str();
      } else if (elem_tag == dwarf::Tag::DW_TAG_inheritance) {
        elem_name = elem_DIT->getBaseType()->getName().str();
      }

      if (elem_name == "") {
        elem_name = "field" + std::to_string(field_idx);
      }
      elem_names->push_back(elem_name);
      field_idx++;
    } else if (isa<DISubprogram>(iter2)) {
      //methods of classes, skip
      continue;
    }
  }
}

void carver_pass::insert_struct_carve_probe_inner(Value * struct_ptr, Type * type) {
  IRBuilderBase::InsertPoint cur_ip = IRB->saveIP();
  StructType * struct_type = dyn_cast<StructType>(type);
  const StructLayout * SL = DL->getStructLayout(struct_type);

  std::string struct_name = struct_type->getName().str();
  struct_name = struct_name.substr(struct_name.find('.') + 1);
  if (struct_name.find("::") != std::string::npos) {
    struct_name = struct_name.substr(struct_name.find("::") + 2);
  }

  std::string struct_carver_name = "__Carv_inner_" + struct_name;
  auto search = struct_carvers.find(struct_carver_name);
  FunctionCallee struct_carver
    = Mod->getOrInsertFunction(struct_carver_name, VoidTy, struct_ptr->getType());

  if (search == struct_carvers.end()) {
    struct_carvers.insert(struct_carver_name);

    //Define struct carver
    Function * struct_carv_func = dyn_cast<Function>(struct_carver.getCallee());

    BasicBlock * entry_BB
      = BasicBlock::Create(*Context, "entry", struct_carv_func);

    IRB->SetInsertPoint(entry_BB);
    IRB->CreateRetVoid();
    IRB->SetInsertPoint(entry_BB->getFirstNonPHIOrDbgOrLifetime());

    BasicBlock * cur_block = entry_BB;

    llvm::errs() << "Getting names of " << struct_name << "\n";

    //Get field names
    bool found_DIType = false;
    std::vector<std::string> elem_names;
    for (auto iter : DbgFinder.types()) {
      if (struct_name == iter->getName().str()) {
        found_DIType = true;
        DIType * dit = iter;
        get_elem_names(dit, &elem_names);
        break;
      }
    }

    if (!found_DIType) {
      //Infer DIType from using DISubprogram
      for (auto DIsubprog : DbgFinder.subprograms()) {
        if (struct_name == DIsubprog->getName().str()) {
          DISubroutineType * DISubroutType = DIsubprog->getType();
          for (auto subtype : DISubroutType->getTypeArray()) {
            if (subtype == NULL) { continue; }

            if (isa<DIDerivedType>(subtype)) {
              DIDerivedType * DIderived_type = dyn_cast<DIDerivedType>(subtype);
              if (DIderived_type->getTag() == dwarf::Tag::DW_TAG_pointer_type) {
                DIType * DItype = DIderived_type->getBaseType();
                if ((DItype != NULL) && isa<DICompositeType>(DItype)) {
                  llvm::errs() << "Using dit taken from chaos .... \n";
                  DItype->dump();
                  get_elem_names(DItype, &elem_names);
                }
              }
            }
          }
          break;
        }
      }
    }

    auto memberoffsets = SL->getMemberOffsets();
    if (elem_names.size() > memberoffsets.size()) {
      DEBUG0("Wrong # of elem names....\n");
      elem_names.clear();
    }

    while (elem_names.size() < memberoffsets.size()) {
      //Can't get field names, just put simple name
      int field_idx = elem_names.size();
      elem_names.push_back("field" + std::to_string(field_idx));
    }
    
    if (elem_names.size() == 0) {
      //struct types with zero fields?
      IRB->restoreIP(cur_ip);
      return;
    }

    Value * carver_param = struct_carv_func->getArg(0);

    int elem_idx = 0;
    for (auto iter : elem_names) {
      Constant * field_name_const = gen_new_string_constant(iter);
      std::vector<Value *> struct_name_probe_args {field_name_const};
      IRB->CreateCall(struct_name_func, struct_name_probe_args);

      Value * gep = IRB->CreateStructGEP(struct_type, carver_param, elem_idx);
      PointerType* gep_type = dyn_cast<PointerType>(gep->getType());
      Type * gep_pointee_type = gep_type->getPointerElementType();

      if (gep_pointee_type->isStructTy()) {
        insert_struct_carve_probe(gep, gep_pointee_type);
        std::vector<Value *> empty_args;
        IRB->CreateCall(name_free_pop, empty_args);
      } else if (gep_pointee_type->isArrayTy()) {
        cur_block = insert_carve_probe(gep, cur_block);
        std::vector<Value *> empty_args {};
        IRB->CreateCall(name_free_pop, empty_args);
      } else if (is_func_ptr_type(gep_pointee_type)) {
        Value * ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, gep, Int8PtrTy);
        std::vector<Value *> probe_args {ptrval};
        IRB->CreateCall(carv_func_ptr, probe_args);
        std::vector<Value *> empty_args {};
        IRB->CreateCall(carv_name_pop, empty_args);
      } else {
        Value * loadval = IRB->CreateLoad(gep_pointee_type, gep);
        cur_block = insert_carve_probe(loadval, cur_block);
        std::vector<Value *> empty_args;
        IRB->CreateCall(carv_name_pop, empty_args);
      }
      elem_idx ++;
    }
  }

  IRB->restoreIP(cur_ip);
  std::vector<Value *> carver_args {struct_ptr};
  IRB->CreateCall(struct_carver, carver_args);
  return;
}

void carver_pass::insert_struct_carve_probe(Value * struct_ptr, Type * type) {

  IRBuilderBase::InsertPoint cur_ip = IRB->saveIP();

  StructType * struct_type = dyn_cast<StructType>(type);
  const StructLayout * SL = DL->getStructLayout(struct_type);

  std::string struct_name = struct_type->getName().str();
  struct_name = struct_name.substr(0, struct_name.find('.'))
    + "_" + struct_name.substr(struct_name.find('.') + 1);

  std::string struct_carver_name = "__Carv__" + struct_name;
  auto search = struct_carvers.find(struct_carver_name);
  FunctionCallee struct_carver
    = Mod->getOrInsertFunction(struct_carver_name, VoidTy, struct_ptr->getType());

  if (search == struct_carvers.end()) {

    struct_carvers.insert(struct_carver_name);

    //Define struct carver
    Function * struct_carv_func = dyn_cast<Function>(struct_carver.getCallee());

    BasicBlock * entry_BB
      = BasicBlock::Create(*Context, "entry", struct_carv_func);

    IRB->SetInsertPoint(entry_BB);
    IRB->CreateRetVoid();
    IRB->SetInsertPoint(entry_BB->getFirstNonPHIOrDbgOrLifetime());

    Value * carver_param = struct_carv_func->getArg(0);

    auto search2 = derived_class_types.find(type);
    if (search2 == derived_class_types.end()) {
      insert_struct_carve_probe_inner(carver_param, type);
    } else {
      Value * casted_var = IRB->CreateCast(Instruction::CastOps::BitCast, carver_param, Int8PtrTy);
      std::vector<Value *> idx_args {casted_var};
      Instruction * name_idx = IRB->CreateCall(get_class_name_idx, idx_args);

      BasicBlock * orig_block = entry_BB->splitBasicBlock(name_idx->getNextNonDebugInstruction());
      int orig_case_id = class_name_map[type].first;
      IRB->SetInsertPoint(orig_block->getFirstNonPHIOrDbgOrLifetime());
      insert_struct_carve_probe_inner(carver_param, type);
      std::vector<Value *> empty_args {};
      IRB->CreateCall(pop_carving_obj, empty_args);
      IRB->CreateRetVoid();
      Instruction * old_term = orig_block->getTerminator();
      old_term->eraseFromParent();

      IRB->SetInsertPoint(name_idx->getNextNonDebugInstruction());
      SwitchInst * switch_inst
        = IRB->CreateSwitch(name_idx, orig_block, search2->second.size() + 1);

      switch_inst->addCase(ConstantInt::get(Int32Ty, orig_case_id), orig_block);

      for (auto derived_type : search2->second) {
        BasicBlock * derived_block = entry_BB->splitBasicBlock(switch_inst->getNextNonDebugInstruction());
        int case_id = class_name_map[derived_type].first;
        switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), derived_block);
        IRB->SetInsertPoint(derived_block->getFirstNonPHIOrDbgOrLifetime());
        
        Value * casted_var= IRB->CreateCast(Instruction::CastOps::BitCast
          , carver_param, PointerType::get(derived_type, 0));

        insert_struct_carve_probe_inner(casted_var, derived_type);
        std::vector<Value *> empty_args {};
        IRB->CreateCall(pop_carving_obj, empty_args);
        IRB->CreateRetVoid();
        Instruction * old_term = derived_block->getTerminator();
        old_term->eraseFromParent();
      }

      Instruction * entry_old_term = entry_BB->getTerminator();
      entry_old_term->eraseFromParent();
    }
  }

  IRB->restoreIP(cur_ip);
  std::vector<Value *> carver_args {struct_ptr};
  IRB->CreateCall(struct_carver, carver_args);

  return;
}

bool carver_pass::hookInstrs(Module &M) {
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
  
  mem_allocated_probe = M.getOrInsertFunction(
    get_link_name("__mem_allocated_probe"), VoidTy, Int8PtrTy, Int32Ty);
  remove_probe = M.getOrInsertFunction(get_link_name("__remove_mem_allocated_probe")
    , VoidTy, Int8PtrTy);
  record_func_ptr = M.getOrInsertFunction(get_link_name("__record_func_ptr"),
    VoidTy, Int8PtrTy, Int8PtrTy);
  argv_modifier = M.getOrInsertFunction(get_link_name("__carver_argv_modifier")
    , VoidTy, Int32PtrTy, Int8PtrPtrPtrTy);
  __carv_fini = M.getOrInsertFunction(get_link_name("__carv_FINI")
    , VoidTy);
  strlen_callee = M.getOrInsertFunction("strlen", Int64Ty, Int8PtrTy);
  carv_char_func = M.getOrInsertFunction(get_link_name("Carv_char")
    , VoidTy, Int8Ty);
  carv_short_func = M.getOrInsertFunction(get_link_name("Carv_short")
    , VoidTy, Int16Ty);
  carv_int_func = M.getOrInsertFunction(get_link_name("Carv_int")
    , VoidTy, Int32Ty);
  carv_long_func = M.getOrInsertFunction(get_link_name("Carv_longtype")
    , VoidTy, Int64Ty);
  carv_longlong_func = M.getOrInsertFunction(get_link_name("Carv_longlong")
    , VoidTy, Int128Ty);
  carv_float_func = M.getOrInsertFunction(get_link_name("Carv_float")
    , VoidTy, FloatTy);
  carv_double_func = M.getOrInsertFunction(get_link_name("Carv_double")
    , VoidTy, DoubleTy);
  carv_ptr_func = M.getOrInsertFunction(get_link_name("Carv_pointer")
    , Int32Ty, Int8PtrTy);
  carv_ptr_name_update = M.getOrInsertFunction(
    get_link_name("__carv_ptr_name_update"), VoidTy, Int32Ty);
  struct_name_func = M.getOrInsertFunction(
    get_link_name("__carv_struct_name_update"), VoidTy, Int8PtrTy);
  carv_name_push = M.getOrInsertFunction(
    get_link_name("__carv_name_push"), VoidTy, Int8PtrTy);
  carv_name_pop = M.getOrInsertFunction(
    get_link_name("__carv_name_pop"), VoidTy);
  name_free_pop = M.getOrInsertFunction(
    get_link_name("__carv_name_free_pop"), VoidTy);
  carv_func_ptr = M.getOrInsertFunction(get_link_name("__Carv_func_ptr")
    , VoidTy, Int8PtrTy);
  carv_func_call = M.getOrInsertFunction(
    get_link_name("__carv_func_call_probe"), VoidTy, Int32Ty);
  carv_func_ret = M.getOrInsertFunction(
    get_link_name("__carv_func_ret_probe"), VoidTy, Int8PtrTy, Int32Ty);
  update_carved_ptr_idx = M.getOrInsertFunction(
    get_link_name("__update_carved_ptr_idx"), VoidTy); 
  mem_alloc_type = M.getOrInsertFunction(
    get_link_name("__mem_alloc_type"), VoidTy, Int8PtrTy, Int8PtrTy);
  keep_class_name = M.getOrInsertFunction(
    get_link_name("__keep_class_name"), VoidTy, Int8PtrTy);
  get_class_name_idx = M.getOrInsertFunction(
    get_link_name("__get_class_name_idx"), Int32Ty, Int8PtrTy);
  pop_carving_obj = M.getOrInsertFunction(
    get_link_name("__pop_carving_obj"), VoidTy);

  get_class_type_info();

  get_instrument_func_set();

  find_global_var_uses();

  DEBUG0("Iterating functions...\n");

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) { continue; }

    std::string func_name = F.getName().str();
    if (instrument_func_set.find(func_name) == instrument_func_set.end()) {
      //Just perform memory tracking
      for (auto &BB : F) {
        for (auto &IN : BB) {
          CallInst * call_instr;
          if ((call_instr = dyn_cast<CallInst>(&IN))) {
            Function * callee = call_instr->getCalledFunction();
            if ((callee == NULL) || (callee->isDebugInfoForProfiling()))
              { continue; }
            std::string callee_name = callee->getName().str();
            Insert_callinst_probe(&IN, callee_name, false);
          }
        }
      }
      continue;
    }

    DEBUG0("Inserting probe in " << func_name << "\n");

    std::vector<Instruction *> cast_instrs;
    std::vector<CallInst *> call_instrs;
    std::vector<Instruction *> ret_instrs;
    for (auto &BB : F) {
      for (auto &IN : BB) {
        if (isa<CastInst>(&IN)) {
          cast_instrs.push_back(&IN);
        } else if (isa<CallInst>(&IN)) {
          call_instrs.push_back(dyn_cast<CallInst>(&IN));
        } else if (isa<ReturnInst>(&IN)) {
          ret_instrs.push_back(&IN);
        }
      }
    }

    BasicBlock& entry_block = F.getEntryBlock();
    Insert_alloca_probe(entry_block);

    IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());
    Constant * func_id_const = ConstantInt::get(Int32Ty, func_id++);
    std::vector<Value *> func_call_args {func_id_const};
    Instruction * init_probe
      = IRB->CreateCall(carv_func_call, func_call_args);

    //Main argc argv handling
    if (func_name == "main") {
      Insert_main_probe(entry_block, F, M.global_values());
      IRB->SetInsertPoint(init_probe->getNextNonDebugInstruction());
    } else if (F.isVarArg()) {
      //TODO
    } else {

      //Insert input carving probes
      int param_idx = 0;
  
      BasicBlock * insert_block = &entry_block;

      for (auto &arg_iter : F.args()) {
        Value * func_arg = &arg_iter;

        std::string param_name = find_param_name(func_arg, insert_block);

        if (param_name == "") {
          param_name = "parm_" + std::to_string(param_idx);
        }

        Constant * param_name_const = gen_new_string_constant(param_name);
        std::vector<Value *> push_args {param_name_const};
        IRB->CreateCall(carv_name_push, push_args);

        insert_block
          = insert_carve_probe(func_arg, insert_block);
        
        std::vector<Value *> pop_args;
        IRB->CreateCall(carv_name_pop, pop_args);
        param_idx ++;
      }

      insert_global_carve_probe(&F, insert_block);
    }

    std::vector<Value *> empty_args;
    IRB->CreateCall(update_carved_ptr_idx, empty_args);

    DEBUG0("Insert memory tracking for " << func_name << "\n");

    //Call instr probing
    for (auto call_instr : call_instrs) {
      //insert new/free probe, return value probe
      Function * callee = call_instr->getCalledFunction();
      if ((callee == NULL) || (callee->isDebugInfoForProfiling()))
        { continue; }
      std::string callee_name = callee->getName().str();
      Insert_callinst_probe(call_instr, callee_name, true);
    }

    //Probing at return
    for (auto ret_instr : ret_instrs) {
      IRB->SetInsertPoint(ret_instr);

      //Write carved result
      Constant * func_name_const = gen_new_string_constant(func_name);

      std::vector<Value *> probe_args {func_name_const, func_id_const};
      IRB->CreateCall(carv_func_ret, probe_args);

      //Remove alloca (local variable) memory tracking info.
      for (auto iter = tracking_allocas.begin();
        iter != tracking_allocas.end(); iter++) {
        AllocaInst * alloc_instr = *iter;

        Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, alloc_instr, Int8PtrTy);
        std::vector<Value *> args {casted_ptr};
        IRB->CreateCall(remove_probe, args);
      }

      //Insert fini
      if (func_name == "main") {
        IRB->CreateCall(__carv_fini, std::vector<Value *>());
      }
    }

    tracking_allocas.clear();

    DEBUG0("done in " << func_name << "\n");
  }

  char * tmp = getenv("DUMP_IR");
  if (tmp) {
    M.dump();
  }

  delete IRB;
  return true;
}

bool carver_pass::runOnModule(Module &M) {

  DEBUG0("Running carver_pass\n");

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

void carver_pass::read_probe_list() {
  std::string file_path = __FILE__;
  file_path = file_path.substr(0, file_path.rfind("/"));
  std::string probe_file_path
    = file_path.substr(0, file_path.rfind("/")) + "/lib/carver_probe_names.txt";
  
  std::ifstream list_file(probe_file_path);

  std::string line;
  while(std::getline(list_file, line)) {
    size_t space_loc = line.find(' ');
    probe_link_names.insert(std::make_pair(line.substr(0, space_loc)
      , line.substr(space_loc + 1)));
  }

  if (probe_link_names.size() == 0) {
    DEBUG0("Can't find lib/carver_probe_names.txt file!\n");
    std::abort();
  }
}

std::string carver_pass::get_link_name(std::string base_name) {
  auto search = probe_link_names.find(base_name);
  if (search == probe_link_names.end()) {
    DEBUG0("Can't find probe name : " << base_name << "! Abort.\n");
    std::abort();
  }

  return search->second;
}

Constant * carver_pass::gen_new_string_constant(std::string name) {

  auto search = new_string_globals.find(name);

  if (search == new_string_globals.end()) {
    Constant * new_global = IRB->CreateGlobalStringPtr(name);
    new_string_globals.insert(std::make_pair(name, new_global));
    return new_global;
  }

  return search->second;
}

void carver_pass::get_instrument_func_set() {
  std::ofstream outfile("funcs.txt");

  for (auto &F : Mod->functions()) {
    if (F.isIntrinsic() || !F.size()) { continue; }
    std::string func_name = F.getName().str();
    if (func_name == "_GLOBAL__sub_I_main.cc") { continue;}
    if (func_name == "__cxx_global_var_init") { continue; }

    //TODO
    if (F.isVarArg()) { continue; }
    
    for (auto iter : DbgFinder.subprograms()) {

      if ((iter->getLinkageName().str() == func_name) || (iter->getName().str() == func_name)) {
        std::string filename = iter->getFilename().str();
        if (filename.find("/usr/bin/") == std::string::npos) {
          outfile << func_name << "\n";
          instrument_func_set.insert(func_name);
        }
        break;
      }
    }
  }

  if (instrument_func_set.find("main") == instrument_func_set.end()) {
    instrument_func_set.insert("main");
    outfile << "main\n";
  }

  outfile.close();
}

std::string carver_pass::find_param_name(Value * param, BasicBlock * BB) {

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

static void registercarver_passPass(const PassManagerBuilder &,
                                           legacy::PassManagerBase &PM) {

  auto p = new carver_pass();
  PM.add(p);

}

static RegisterStandardPasses Registercarver_passPass(
    PassManagerBuilder::EP_OptimizerLast, registercarver_passPass);

static RegisterStandardPasses Registercarver_passPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registercarver_passPass);

static RegisterStandardPasses Registercarver_passPassLTO(
    PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
    registercarver_passPass);

