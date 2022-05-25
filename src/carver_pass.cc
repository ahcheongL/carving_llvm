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
  void Insert_memfunc_probe(Instruction& IN, std::string callee_name);
  void Insert_main_probe(BasicBlock & entry_block, Function & F
    , global_range globals);
  BasicBlock * insert_carve_probe(Value * val, std::string name
    , BasicBlock * BB);

  std::set<std::string> struct_carvers;
  void insert_struct_carve_probe(Value * struct_ptr, Type * struct_type
    , std::string name);
  
  int insert_global_carve_probe(Function * F, BasicBlock * BB);

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
  
  FunctionCallee mem_allocated_probe;
  FunctionCallee remove_probe;
  FunctionCallee record_func_ptr;
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
  FunctionCallee carv_ptr_func;
  FunctionCallee carv_ptr_update;
  FunctionCallee carv_ptr_done;
  FunctionCallee carv_func_ptr;

  int func_id;
};

}  // namespace

char carver_pass::ID = 0;

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
    } else if (allocas.size() != 0) {
      //We met not-alloca instruction
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

        Value * size_const = ConstantInt::get(Int32Ty, size);
        std::vector<Value *> args {casted_ptr, size_const};
        IRB->CreateCall(mem_allocated_probe, args);
        tracking_allocas.push_back(alloc_instr);
      }
      break;
    }
  }
}

void carver_pass::Insert_memfunc_probe(Instruction& IN, std::string callee_name) {
  IRB->SetInsertPoint(IN.getNextNonDebugInstruction());
  
  if (callee_name == "malloc") {
    //Track malloc
    Value * size = IN.getOperand(0);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {&IN, size};
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
    Value * size = IN.getOperand(2);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {IN.getOperand(0), size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "llvm.memmove.p0i8.p0i8.i64") {
    Value * size = IN.getOperand(2);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {IN.getOperand(0), size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strlen") {
    Value * add_one = IRB->CreateAdd(&IN, ConstantInt::get(Int64Ty, 1));
    Value * size = IRB->CreateCast(Instruction::CastOps::Trunc, add_one, Int32Ty);
    std::vector<Value *> args {IN.getOperand(0), size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strncpy") {
    Value * size = IN.getOperand(2);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {IN.getOperand(0), size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "strcpy") {
    std::vector<Value *> strlen_args;
    strlen_args.push_back(IN.getOperand(0));
    Value * strlen_result = IRB->CreateCall(strlen_callee, strlen_args);
    Value * add_one = IRB->CreateAdd(strlen_result, ConstantInt::get(Int64Ty, 1));
    std::vector<Value *> args {IN.getOperand(0), add_one};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "_Znwm") {
    //new operator
    Value * size = IN.getOperand(0);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {&IN, size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "_ZdlPv") {
    //delete operator
    std::vector<Value *> args {IN.getOperand(0)};
    IRB->CreateCall(remove_probe, args);
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

  if (F.arg_size() == 2) {
    Constant * argc_name_const = gen_new_string_constant("argc");
    std::vector<Value *> probe_args1 {new_argc, argc_name_const};
    IRB->CreateCall(carv_int_func, probe_args1);

    BasicBlock * insert_block
      = insert_carve_probe(new_argv, "argv", IRB->GetInsertBlock());

    insert_global_carve_probe(&F, insert_block);

    Constant * func_name_const = gen_new_string_constant("main");
    Constant * func_id_const = ConstantInt::get(Int32Ty, func_id++);

    std::vector<Value *> probe_args {func_name_const, func_id_const};
    IRB->CreateCall(write_carved, probe_args);
  }

  //Record func ptr
  for (auto &Func:Mod->functions()) {
    if (Func.size() == 0) { continue; }
    Constant * func_name_const = gen_new_string_constant(Func.getName().str());
    Value * cast_val = IRB->CreateCast(Instruction::CastOps::BitCast
      , (Value *) &Func, Int8PtrTy);
    std::vector<Value *> probe_args {cast_val, func_name_const};
    IRB->CreateCall(record_func_ptr, probe_args);
  }

  return;
}

int carver_pass::insert_global_carve_probe(Function * F, BasicBlock * BB) {

  BasicBlock * cur_block = BB;
  int num_inserted = 0;

  auto search = global_var_uses.find(F);
  if (search != global_var_uses.end()) {
    for (auto glob_iter : search->second) {
      std::string glob_name = glob_iter->getName().str();

      Type * const_type = glob_iter->getType();
      assert(const_type->isPointerTy());
      Type * pointee_type = dyn_cast<PointerType>(const_type)->getPointerElementType();

      if (pointee_type->isStructTy()) {
        insert_struct_carve_probe((Value *) glob_iter, pointee_type, glob_name);
      } else {
        Value * load_val = IRB->CreateLoad(pointee_type, (Value *) glob_iter);
        cur_block = insert_carve_probe(load_val, glob_name, cur_block);
      }
      num_inserted++;
    }
  }

  return num_inserted;
}

BasicBlock * carver_pass::insert_carve_probe(Value * val, std::string name
  , BasicBlock * BB) {
  Constant * name_constant = gen_new_string_constant(name);
  Type * val_type = val->getType();

  std::vector<Value *> probe_args {val, name_constant};
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
  } else if (val_type->isFunctionTy()) {
    //IRB->CreateCall(carv_func_ptr, probe_args);
  } else if (val_type->isPointerTy()) {
    PointerType * ptrtype = dyn_cast<PointerType>(val_type);
    //type that we don't know.
    if (ptrtype->isOpaque() || ptrtype->isOpaquePointerTy()) { return BB; }

    Type * pointee_type = ptrtype->getPointerElementType();

    if (pointee_type->isFunctionTy()) {
      Value * ptrval = val;
      if (val_type != Int8PtrTy) {
        ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, val, Int8PtrTy);
      }
      Constant * name_constant = gen_new_string_constant(name + "[]");
      std::vector<Value *> probe_args {ptrval, name_constant};
      IRB->CreateCall(carv_func_ptr, probe_args);
      return BB;
    }

    if (isa<StructType> (pointee_type)) {
      StructType * tmptype = dyn_cast<StructType>(pointee_type);
      if (tmptype->isOpaque()) { return BB; }
    }

    unsigned pointee_size = DL->getTypeAllocSize(pointee_type);
    if (pointee_size == 0) { return BB; }

    //get 0 initialized index
    Instruction * index_alloc = IRB->CreateAlloca(Int32Ty);
    Value * index_store = IRB->CreateStore(ConstantInt::get(Int32Ty, 0), index_alloc);
    Value * index_load = IRB->CreateLoad(Int32Ty, index_alloc);

    Value * ptrval = val;
    if (val_type != Int8PtrTy) {
      ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, val, Int8PtrTy);
    }

    //Call Carv_pointer
    std::vector<Value *> probe_args {ptrval, name_constant};
    Instruction * carv_ptr = IRB->CreateCall(carv_ptr_func, probe_args);

    
    Instruction * pointer_size = (Instruction *)
      IRB->CreateSDiv(carv_ptr, ConstantInt::get(Int32Ty, pointee_size));

    //Make loop block
    BasicBlock * loopblock = BB->splitBasicBlock(pointer_size->getNextNonDebugInstruction());
    BasicBlock * const loopblock_start = loopblock;

    IRB->SetInsertPoint(pointer_size->getNextNonDebugInstruction());
    
    Instruction * cmp_instr1 = (Instruction *)
      IRB->CreateICmpEQ(pointer_size, ConstantInt::get(Int32Ty, 0));

    IRB->SetInsertPoint(loopblock->getFirstNonPHIOrDbgOrLifetime());
    PHINode * index_phi = IRB->CreatePHI(Int32Ty, 2);
    index_phi->addIncoming(index_load, BB);
    Value * getelem_instr = IRB->CreateGEP(pointee_type, val, index_phi);

    if (pointee_type->isStructTy()) {
      insert_struct_carve_probe(getelem_instr, pointee_type, name + "[]");
    } else {
      Value * load_ptr = IRB->CreateLoad(pointee_type, getelem_instr);
      loopblock = insert_carve_probe(load_ptr, name + "[]", loopblock);
    }

    Value * index_update_instr
      = IRB->CreateAdd(index_phi, ConstantInt::get(Int32Ty, 1));
    index_phi->addIncoming(index_update_instr, loopblock);

    std::vector<Value *> probe_args2 {ptrval};
    IRB->CreateCall(carv_ptr_update, probe_args2);

    Instruction * cmp_instr2
      = (Instruction *) IRB->CreateICmpSLT(index_update_instr, pointer_size);

    BasicBlock * endblock
      = loopblock->splitBasicBlock(cmp_instr2->getNextNonDebugInstruction());

    IRB->SetInsertPoint(cmp_instr1->getNextNonDebugInstruction());

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

    std::vector<Value *> probe_args3 {ptrval};
    IRB->CreateCall(carv_ptr_done, probe_args3);

    return endblock;
  }

  return BB;
}

void carver_pass::insert_struct_carve_probe(Value * struct_ptr, Type * type
  , std::string name) {

  IRBuilderBase::InsertPoint cur_ip = IRB->saveIP();

  StructType * struct_type = dyn_cast<StructType>(type);
  const StructLayout * SL = DL->getStructLayout(struct_type);

  std::string struct_name = struct_type->getName().str();
  struct_name = struct_name.substr(struct_name.find('.') + 1);

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

    BasicBlock * cur_block = entry_BB;

    //Get field names
    std::vector<std::string> elem_names;
    for (auto iter : DbgFinder.types()) {
      if (struct_name == iter->getName().str()) {
        DIType * dit = iter;
        while (isa<DIDerivedType>(dit) || isa<DISubroutineType>(dit)) {
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
          DEBUG0("Warn : unknown DIType : \n");
          iter->dump();
          break;
        }

        DICompositeType * struct_DIT = dyn_cast<DICompositeType>(dit);
        int field_idx = 0;
        for (auto iter2 : struct_DIT->getElements()) {
          iter2->dump();
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
            elem_names.push_back(elem_name);
            field_idx++;
          } else if (isa<DISubprogram>(iter2)) {
            //methods of classes, skip
            continue;
          }
        }
        break;
      }
    }

    if (elem_names.size() == 0) {
      //Can't get field names, just put simple name
      int field_index = 0;
      for (auto _field : SL->getMemberOffsets()) {
        elem_names.push_back("field" + std::to_string(field_index++));
      }
    }
    
    if (elem_names.size() == 0) {
      IRB->restoreIP(cur_ip);
      return;
    }

    Value * carver_param = struct_carv_func->getArg(0);

    int elem_idx = 0;
    for (auto iter : elem_names) {
      Value * gep = IRB->CreateStructGEP(struct_type, carver_param, elem_idx);
      PointerType* gep_type = dyn_cast<PointerType>(gep->getType());
      Type * gep_pointee_type = gep_type->getPointerElementType();
      if (gep_pointee_type->isStructTy()) {
        insert_struct_carve_probe(gep, gep_pointee_type, name + "." + iter);
      } else {
        Value * loadval = IRB->CreateLoad(gep_pointee_type, gep);
        cur_block = insert_carve_probe(loadval, name + "." + iter, cur_block);
      }
      elem_idx ++;
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
  write_carved = M.getOrInsertFunction(get_link_name("__write_carved")
    , VoidTy, Int8PtrTy, Int32Ty);
  __carv_fini = M.getOrInsertFunction(get_link_name("__carv_FINI")
    , VoidTy);
  strlen_callee = M.getOrInsertFunction("strlen", Int64Ty, Int8PtrTy);
  carv_char_func = M.getOrInsertFunction(get_link_name("Carv_char")
    , VoidTy, Int8Ty, Int8PtrTy);
  carv_short_func = M.getOrInsertFunction(get_link_name("Carv_short")
    , VoidTy, Int16Ty, Int8PtrTy);
  carv_int_func = M.getOrInsertFunction(get_link_name("Carv_int")
    , VoidTy, Int32Ty, Int8PtrTy);
  carv_long_func = M.getOrInsertFunction(get_link_name("Carv_long")
    , VoidTy, Int64Ty, Int8PtrTy);
  carv_longlong_func = M.getOrInsertFunction(get_link_name("Carv_longlong")
    , VoidTy, Int128Ty, Int8PtrTy);
  carv_float_func = M.getOrInsertFunction(get_link_name("Carv_float")
    , VoidTy, FloatTy, Int8PtrTy);
  carv_double_func = M.getOrInsertFunction(get_link_name("Carv_double")
    , VoidTy, DoubleTy, Int8PtrTy);
  carv_ptr_func = M.getOrInsertFunction(get_link_name("Carv_pointer")
    , Int32Ty, Int8PtrTy, Int8PtrTy );
  carv_ptr_update = M.getOrInsertFunction(get_link_name("__carv_pointer_idx_update")
    , VoidTy, Int8PtrTy);
  carv_ptr_done = M.getOrInsertFunction(get_link_name("__carv_pointer_done")
    , VoidTy, Int8PtrTy);
  carv_func_ptr = M.getOrInsertFunction(get_link_name("__Carv_func_ptr")
    , VoidTy, Int8PtrTy, Int8PtrTy);

  get_instrument_func_set();

  find_global_var_uses();

  DEBUG0("Iterating functions...\n");

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) { continue; }

    std::string func_name = F.getName().str();
    if (instrument_func_set.find(func_name) == instrument_func_set.end()) {
      continue;
    }

    DEBUG0("Inserting probe in " << func_name << "\n");

    BasicBlock& entry_block = F.getEntryBlock();
    Insert_alloca_probe(entry_block);

    //Main argc argv handling
    if (func_name == "main") {
      Insert_main_probe(entry_block, F, M.global_values());
    } else {
      int param_idx = 0;
      IRB->SetInsertPoint(entry_block.getFirstNonPHIOrDbgOrLifetime());

      BasicBlock * insert_block = &entry_block;

      for (auto arg_iter = F.arg_begin(); arg_iter != F.arg_end(); arg_iter++) {
        Value * func_arg = &(*arg_iter);

        std::string param_name = find_param_name(func_arg, insert_block);

        if (param_name == "") {
          param_name = "parm_" + std::to_string(param_idx);
        }

        insert_block
          = insert_carve_probe(func_arg, param_name, insert_block);
        param_idx ++;
      }

      int num_inserted = insert_global_carve_probe(&F, insert_block);

      if ((num_inserted > 0) || (param_idx > 0)) {
        //Call __write_carved
        Constant * func_name_const = gen_new_string_constant(func_name);
        Constant * func_id_const = ConstantInt::get(Int32Ty, func_id++);

        std::vector<Value *> probe_args {func_name_const, func_id_const};
        IRB->CreateCall(write_carved, probe_args);
      }
    }

    DEBUG0("Insert memory tracking for " << func_name << "\n");

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

            Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, alloc_instr, Int8PtrTy);
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
    
    for (auto iter : DbgFinder.subprograms()) {
      if ((iter->getLinkageName().str() == func_name) || (iter->getName().str() == func_name)) {
        std::string filename = iter->getFilename().str();
        llvm::errs() << "filename : " << filename << "\n";
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

