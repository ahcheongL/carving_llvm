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
  bool get_target_func();

  std::map<std::string, std::string> probe_link_names;
  void read_probe_list();
  std::string get_link_name(std::string);

  std::string find_param_name(Value * param, BasicBlock * BB);

  std::map<std::string, Constant *> new_string_globals;
  Constant * gen_new_string_constant(std::string name);

  std::pair<BasicBlock *, Value *> insert_replay_probe(Type *, BasicBlock *);

  std::set<std::string> struct_replayes;
  void insert_struct_replay_probe(Value*, Type *);

  std::set<BasicBlock *> replay_BBs;
  
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

  FunctionCallee replay_char_func;
  FunctionCallee replay_short_func;
  FunctionCallee replay_int_func;
  FunctionCallee replay_long_func;
  FunctionCallee replay_longlong_func;
  FunctionCallee replay_float_func;
  FunctionCallee replay_double_func;
  FunctionCallee replay_ptr_func;
  FunctionCallee replay_ptr_alloc_size;
  FunctionCallee replay_ptr_done;
  FunctionCallee replay_func_ptr;

  FunctionCallee record_func_ptr;


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

  replay_char_func = M.getOrInsertFunction(get_link_name("Replay_char")
    , Int8Ty);
  replay_short_func = M.getOrInsertFunction(get_link_name("Replay_short")
    , Int16Ty);
  replay_int_func = M.getOrInsertFunction(get_link_name("Replay_int")
    , Int32Ty);
  replay_long_func = M.getOrInsertFunction(get_link_name("Replay_long")
    , Int64Ty);
  replay_longlong_func = M.getOrInsertFunction(get_link_name("Replay_longlong")
    , Int128Ty);
  replay_float_func = M.getOrInsertFunction(get_link_name("Replay_float")
    , FloatTy);
  replay_double_func = M.getOrInsertFunction(get_link_name("Replay_double")
    , DoubleTy);
  replay_ptr_func = M.getOrInsertFunction(get_link_name("Replay_pointer")
    , Int8PtrTy);
  replay_ptr_alloc_size = M.getOrInsertFunction(get_link_name("Replay_ptr_alloc_size")
    , Int32Ty);

  replay_func_ptr = M.getOrInsertFunction(get_link_name("Replay_func_ptr")
    , Int8PtrTy);

  record_func_ptr = M.getOrInsertFunction(get_link_name("__record_func_ptr"),
    VoidTy, Int8PtrTy, Int8PtrTy);

  bool res = get_target_func();

  if (res == false) {
    return true;
  }

  DEBUG0("Iterating functions...\n");

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) { continue; }

    std::string func_name = F.getName().str();

    if (func_name == "main") {
      main_func = &F;
    } else if (target_func != &F) {
      //F.deleteBody();
      //TODO : make stub
    }
  }

  BasicBlock * cur_block = &(main_func->getEntryBlock());
  replay_BBs.insert(cur_block);

  IRB->SetInsertPoint(cur_block->getFirstNonPHIOrDbgOrLifetime());

  //Record func ptr
  for (auto &Func : M.functions()) {
    if (Func.size() == 0) { continue; }
    Constant * func_name_const = gen_new_string_constant(Func.getName().str());
    Value * cast_val = IRB->CreateCast(Instruction::CastOps::BitCast
      , (Value *) &Func, Int8PtrTy);
    std::vector<Value *> probe_args {cast_val, func_name_const};
    IRB->CreateCall(record_func_ptr, probe_args);
  }

  Value * argv = main_func->getArg(1);
  std::vector<Value *> reader_args {argv};
  IRB->CreateCall(__inputf_reader, reader_args);

  if (target_func == main_func) {
    //TODO
    
  } else {
    std::vector<Value *> target_args;
    for (auto &arg : target_func->args()) {
      Type * arg_type = arg.getType();
      auto replay_res = insert_replay_probe(arg_type, cur_block);
      cur_block = replay_res.first;
      target_args.push_back(replay_res.second);
    }

    Instruction * target_call = IRB->CreateCall(
      target_func->getFunctionType(), target_func, target_args);
    target_call->setDebugLoc(DebugLoc());

    //Return
    cur_block->splitBasicBlock(target_call->getNextNonDebugInstruction());
    IRB->SetInsertPoint(target_call->getNextNonDebugInstruction());

    Instruction * new_main_term = IRB->CreateRet(ConstantInt::get(Int32Ty, 0));
    new_main_term->removeFromParent();
    Instruction * old_term = cur_block->getTerminator();
    ReplaceInstWithInst(old_term, new_main_term);

    //remove other BB
    std::vector<BasicBlock *> BBs;
    for (auto &BB: main_func->getBasicBlockList()) {
      if (replay_BBs.find(&BB) == replay_BBs.end()) {
        BBs.push_back(&BB);
      }
    }

    for (auto BB: BBs) {
      BB->eraseFromParent();
    }
  }

  char * tmp = getenv("DUMP_IR");
  if (tmp) {
    llvm::errs() << "Dumping IR...\n";
    M.dump();
  }

  delete IRB;
  return true;
}

std::pair<BasicBlock *, Value *>
  driver_pass::insert_replay_probe (Type * typeptr, BasicBlock * BB) {
  std::vector<Value *> probe_args;
  BasicBlock * cur_block = BB;
  Value * result = NULL;

  if (typeptr == Int8Ty) {
    result = IRB->CreateCall(replay_char_func, probe_args);
  } else if (typeptr == Int16Ty) {
    result = IRB->CreateCall(replay_short_func, probe_args);
  } else if (typeptr == Int32Ty) {
    result = IRB->CreateCall(replay_int_func, probe_args);
  } else if (typeptr == Int64Ty) {
    result = IRB->CreateCall(replay_long_func, probe_args);
  } else if (typeptr == Int128Ty) {
    result = IRB->CreateCall(replay_longlong_func, probe_args);
  } else if (typeptr == FloatTy) {
    result = IRB->CreateCall(replay_float_func, probe_args);
  } else if (typeptr == DoubleTy) {
    result = IRB->CreateCall(replay_double_func, probe_args);
  } else if (typeptr->isPointerTy()) {
    PointerType * ptrtype = dyn_cast<PointerType>(typeptr);

    if (ptrtype->isOpaque() || ptrtype->isOpaquePointerTy()) {
      return std::make_pair(cur_block , result);
    }

    Type * pointee_type = ptrtype->getPointerElementType();

    if (pointee_type->isFunctionTy()) {
      result = IRB->CreateCall(replay_func_ptr, probe_args);
      result = IRB->CreateCast(Instruction::CastOps::BitCast, result, typeptr);
      return std::make_pair(cur_block , result);
    }

    if (isa<StructType> (pointee_type)) {
      StructType * tmptype = dyn_cast<StructType>(pointee_type);
      if (tmptype->isOpaque()) { return std::make_pair(cur_block , result); }
    }

    unsigned pointee_size = DL->getTypeAllocSize(pointee_type);
    if (pointee_size == 0) { return std::make_pair(cur_block , result); }

    result = IRB->CreateCall(replay_ptr_func, probe_args);

    if (typeptr != Int8PtrTy) {
      result = IRB->CreatePointerCast(result, typeptr);
    }

    //get 0 initialized index
    Instruction * index_alloc = IRB->CreateAlloca(Int32Ty);
    Value * index_store = IRB->CreateStore(ConstantInt::get(Int32Ty, 0), index_alloc);
    Value * index_load = IRB->CreateLoad(Int32Ty, index_alloc);
    
    Instruction * ptr_bytesize = IRB->CreateCall(replay_ptr_alloc_size, probe_args);
    Instruction * ptr_size = (Instruction *) IRB->CreateSDiv(ptr_bytesize
      , ConstantInt::get(Int32Ty, pointee_size));
    
    //Make loop block
    BasicBlock * loopblock = cur_block->splitBasicBlock(ptr_size->getNextNonDebugInstruction());
    BasicBlock * const loopblock_start = loopblock;
    replay_BBs.insert(loopblock);

    IRB->SetInsertPoint(ptr_size->getNextNonDebugInstruction());
    
    Instruction * cmp_instr1 = (Instruction *)
      IRB->CreateICmpEQ(ptr_size, ConstantInt::get(Int32Ty, 0));

    IRB->SetInsertPoint(loopblock->getFirstNonPHIOrDbgOrLifetime());
    PHINode * index_phi = IRB->CreatePHI(Int32Ty, 2);
    index_phi->addIncoming(index_load, cur_block);
    Value * getelem_instr = IRB->CreateGEP(pointee_type, result, index_phi);

    if (pointee_type->isStructTy()) {
      insert_struct_replay_probe(getelem_instr, pointee_type);
    } else {
      auto ptr_result = insert_replay_probe(pointee_type, loopblock);
      loopblock = ptr_result.first;
      Value * ptr_replay_res = ptr_result.second;
      IRB->CreateStore(ptr_replay_res, getelem_instr);
    }

    Value * index_update_instr
      = IRB->CreateAdd(index_phi, ConstantInt::get(Int32Ty, 1));
    index_phi->addIncoming(index_update_instr, loopblock);

    Instruction * cmp_instr2
      = (Instruction *) IRB->CreateICmpSLT(index_update_instr, ptr_size);

    BasicBlock * endblock
      = loopblock->splitBasicBlock(cmp_instr2->getNextNonDebugInstruction());
    replay_BBs.insert(endblock);

    IRB->SetInsertPoint(cmp_instr1->getNextNonDebugInstruction());

    Instruction * BB_term
      = IRB->CreateCondBr(cmp_instr1, endblock, loopblock_start);
    BB_term->removeFromParent();
    
    //remove old terminator
    Instruction * old_term = cur_block->getTerminator();
    ReplaceInstWithInst(old_term, BB_term);

    IRB->SetInsertPoint(cmp_instr2->getNextNonDebugInstruction());

    Instruction * loopblock_term
      = IRB->CreateCondBr(cmp_instr2, loopblock_start, endblock);

    loopblock_term->removeFromParent();

    //remove old terminator
    old_term = loopblock->getTerminator();
    ReplaceInstWithInst(old_term, loopblock_term);

    IRB->SetInsertPoint(endblock->getFirstNonPHIOrDbgOrLifetime());

    cur_block = endblock;
  } else if(typeptr->isArrayTy()) {

    
  } else {
    typeptr->dump();
    DEBUG0("Unknown type\n");
  }

  return std::make_pair(cur_block , result);
}

void driver_pass::insert_struct_replay_probe(Value * ptr, Type * typeptr) {
  StructType * struct_type = dyn_cast<StructType>(typeptr);
  assert(struct_type && "typeptr need to be struct type");

  IRBuilderBase::InsertPoint cur_ip = IRB->saveIP();

  std::string struct_name = struct_type->getName().str();
  struct_name = struct_name.substr(struct_name.find('.') + 1);

  std::string struct_replay_name = "__Replay__" + struct_name;
  auto search = struct_replayes.find(struct_replay_name);
  FunctionCallee struct_replay
    = Mod->getOrInsertFunction(struct_replay_name, VoidTy, ptr->getType());
  if (search == struct_replayes.end()) {
    struct_replayes.insert(struct_replay_name);

    llvm::errs() << "Defininig struct replay for " << struct_name << "\n";

    //Define struct carver
    Function * struct_replay_func = dyn_cast<Function>(struct_replay.getCallee());

    BasicBlock * entry_BB
      = BasicBlock::Create(*Context, "entry", struct_replay_func);

    IRB->SetInsertPoint(entry_BB);
    IRB->CreateRetVoid();
    IRB->SetInsertPoint(entry_BB->getFirstNonPHIOrDbgOrLifetime());

    BasicBlock * cur_block = entry_BB;
    Value * replay_param = struct_replay_func->getArg(0);

    unsigned num_fields = struct_type->getNumElements();

    for (unsigned elem_idx = 0; elem_idx < num_fields; elem_idx++) {
      Value * gep = IRB->CreateStructGEP(struct_type, replay_param, elem_idx);
      Type * field_type = struct_type->getElementType(elem_idx);

      if (field_type->isStructTy()) {
        insert_struct_replay_probe(gep, field_type);
      } else {
        auto ptr_result = insert_replay_probe(field_type, cur_block);
        cur_block = ptr_result.first;
        Value * ptr_replay_res = ptr_result.second;
        if (ptr_replay_res != NULL) {
          IRB->CreateStore(ptr_replay_res, gep);
        }
      }
    }
  }

  IRB->restoreIP(cur_ip);
  std::vector<Value *> replay_args {ptr};
  IRB->CreateCall(struct_replay, replay_args);
  return;
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

bool driver_pass::get_target_func() {
  std::ifstream funcnames("funcs.txt");
  int func_idx = 0;
  char * func_idx_env = getenv("FUNCIDX");
  if (func_idx_env != NULL) {
    func_idx = atoi(func_idx_env);
  }

  llvm::errs() << "func_idx_env : " << func_idx_env << "\n";

  std::string line;
  int line_idx = 0;
  while(std::getline(funcnames, line)) {
    if (line_idx == func_idx) {
      target_name = line;
      DEBUG0("Target name : " << target_name << "\n");
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

  if (target_func == NULL) {
    llvm::errs() << "Can't find target\n";
    llvm::errs() << "func_idx_env : " << func_idx_env << "\n";
    return false;
  }

  return true;
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

