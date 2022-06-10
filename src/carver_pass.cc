#include"pass.hpp"

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

  std::map<std::string, std::string> probe_link_names;

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

  int num_class_name_const = 0;
  std::vector<std::pair<Constant *, int>> class_name_consts;
  std::map<StructType *, std::pair<int, Constant *>> class_name_map;
  void get_class_type_info();

  void gen_class_carver();
  
  DebugInfoFinder DbgFinder;
  Module * Mod;
  LLVMContext * Context;
  const DataLayout * DL;
  IRBuilder<> *IRB;

  Type        *VoidTy;
  IntegerType *Int1Ty;
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
  FunctionCallee get_class_idx;
  FunctionCallee get_class_size;
  FunctionCallee class_carver;
  FunctionCallee update_class_ptr;

  int func_id;
};

}  // namespace

char carver_pass::ID = 0;

void carver_pass::get_class_type_info() {

  //Set dummy location...
  for (auto &F : Mod->functions()) {
    std::string func_name = F.getName().str();
    if (func_name == "main") {
      IRB->SetInsertPoint(F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
      break;
    }
  }

  for (auto struct_type : Mod->getIdentifiedStructTypes()) {
    std::string name = get_type_str(struct_type);
    Constant * name_const = gen_new_string_constant(name, IRB);
    if (struct_type->isOpaque()) { continue; }
    class_name_consts.push_back(std::make_pair(name_const, DL->getTypeAllocSize(struct_type)));
    class_name_map.insert(std::make_pair(struct_type
      , std::make_pair(num_class_name_const++, name_const)));
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
  
}

void carver_pass::gen_class_carver() {
  class_carver
    = Mod->getOrInsertFunction("__class_carver", VoidTy, Int8PtrTy, Int32Ty);
  Function * class_carver_func = dyn_cast<Function>(class_carver.getCallee());
  
  BasicBlock * entry_BB
    = BasicBlock::Create(*Context, "entry", class_carver_func);

  BasicBlock * default_BB
    = BasicBlock::Create(*Context, "default", class_carver_func);
  IRB->SetInsertPoint(default_BB);
  IRB->CreateRetVoid();


  IRB->SetInsertPoint(entry_BB);

  Value * carving_ptr = class_carver_func->getArg(0);
  Value * class_idx = class_carver_func->getArg(1);

  SwitchInst * switch_inst
    = IRB->CreateSwitch(class_idx, default_BB, num_class_name_const);

  for (auto class_type : class_name_map) {
    int case_id = class_type.second.first;
    BasicBlock * case_block = BasicBlock::Create(*Context, std::to_string(case_id), class_carver_func);
    switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
    IRB->SetInsertPoint(case_block);

    StructType * class_type_ptr = class_type.first;
    
    Value * casted_var= IRB->CreateCast(Instruction::CastOps::BitCast
      , carving_ptr, PointerType::get(class_type_ptr, 0));

    insert_struct_carve_probe_inner(casted_var, class_type_ptr);
    IRB->CreateRetVoid();
  }

  return;
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
          std::string typestr = get_type_str(allocated_type);         
          Constant * typename_const = gen_new_string_constant(typestr, IRB);
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
  } else if ((callee_name == "_Znwm") || (callee_name == "_Znam")) {
    //new operator
    CastInst * cast_instr;
    if ((cast_instr = dyn_cast<CastInst>(IN->getNextNonDebugInstruction()))) {
      Type * cast_type = cast_instr->getType();
      if (isa<PointerType>(cast_type)) {
        PointerType * cast_ptr_type = dyn_cast<PointerType>(cast_type);
        Type * pointee_type = cast_ptr_type->getPointerElementType();
        if (pointee_type->isStructTy()) {
          std::string typestr = get_type_str(pointee_type);
          Constant * typename_const = gen_new_string_constant(typestr, IRB);
          std::vector<Value *> args1 {IN, typename_const};
          IRB->CreateCall(mem_alloc_type, args1);
        }
      }
    }

    Value * size = IN->getOperand(0);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {IN, size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if ((callee_name == "_ZdlPv") || (callee_name == "_ZdaPv")) {
    //delete operator
    std::vector<Value *> args {IN->getOperand(0)};
    IRB->CreateCall(remove_probe, args);
  } else if (insert_ret_probe) {
    Type * ret_type = IN->getType();
    if (ret_type != VoidTy) {
      Constant * name_const = gen_new_string_constant("\"" + callee_name + "\"_ret", IRB);
      std::vector<Value *> push_args {name_const};
      IRB->CreateCall(carv_name_push, push_args);

      insert_carve_probe(IN, IN->getParent());

      if (ret_type->isStructTy()) {
        IRB->CreateCall(name_free_pop, empty_args);
      } else {
        IRB->CreateCall(carv_name_pop, empty_args);
      }
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
    Constant * func_name_const = gen_new_string_constant(Func.getName().str(), IRB);
    Value * cast_val = IRB->CreateCast(Instruction::CastOps::BitCast
      , (Value *) &Func, Int8PtrTy);
    std::vector<Value *> probe_args {cast_val, func_name_const};
    IRB->CreateCall(record_func_ptr, probe_args);
  }

  //Record class type string constants
  for (auto iter : class_name_consts) {
    std::vector<Value *> args {iter.first, ConstantInt::get(Int32Ty, iter.second)};
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

      Constant * glob_name_const = gen_new_string_constant(glob_name, IRB);
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
  if (val_type == Int1Ty) {
    Value * cast_val = IRB->CreateZExt(val, Int8Ty);
    std::vector<Value *> probe_args {cast_val};
    IRB->CreateCall(carv_char_func, probe_args);
  } else if (val_type == Int8Ty) {
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
  } else if (val_type->isStructTy()) {
    //Sould be very simple tiny struct...
    StructType * struct_type = dyn_cast<StructType>(val_type);
    const StructLayout * SL = DL->getStructLayout(struct_type);

    BasicBlock * cur_block = BB;

    auto memberoffsets = SL->getMemberOffsets();
    int idx = 0;
    for (auto _iter : memberoffsets) {
      std::string field_name = "field" + std::to_string(idx);
      Value * extracted_val = IRB->CreateExtractValue(val, idx);
      IRB->CreateCall(struct_name_func, gen_new_string_constant(field_name, IRB));
      cur_block = insert_carve_probe(extracted_val, cur_block);
      IRB->CreateCall(carv_name_pop, empty_args);
      idx++;
    }

    return cur_block;
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

    if (pointee_type->isArrayTy()) {
      unsigned array_size = pointee_size;
      //TODO : pointee type == class type

      ArrayType * arrtype = dyn_cast<ArrayType>(pointee_type);
      Type * array_elem_type = arrtype->getArrayElementType();
      val_type = PointerType::get(array_elem_type, 0);
      Instruction * casted_val
        = (Instruction *) IRB->CreateCast(Instruction::CastOps::BitCast, val, val_type);
      pointee_size = DL->getTypeAllocSize(array_elem_type);
      
      Value * pointer_size = ConstantInt::get(Int32Ty, array_size / pointee_size);
      //Make loop block
      BasicBlock * loopblock = BB->splitBasicBlock(casted_val->getNextNonDebugInstruction());
      BasicBlock * const loopblock_start = loopblock;

      IRB->SetInsertPoint(loopblock->getFirstNonPHIOrDbgOrLifetime());
      PHINode * index_phi = IRB->CreatePHI(Int32Ty, 2);
      index_phi->addIncoming(ConstantInt::get(Int32Ty, 0), BB);

      Value * getelem_instr = IRB->CreateGEP(array_elem_type, casted_val, index_phi);

      std::vector<Value *> probe_args2 {index_phi};
      IRB->CreateCall(carv_ptr_name_update, probe_args2);

      if (array_elem_type->isStructTy()) {
        insert_struct_carve_probe(getelem_instr, array_elem_type);
        IRB->CreateCall(name_free_pop, empty_args);
      } else if (is_func_ptr_type(array_elem_type)) {
        Value * ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, casted_val, Int8PtrTy);
        std::vector<Value *> probe_args {ptrval};
        IRB->CreateCall(carv_func_ptr, probe_args);
        IRB->CreateCall(carv_name_pop, empty_args);
      } else {
        Value * load_ptr = IRB->CreateLoad(array_elem_type, getelem_instr);
        loopblock = insert_carve_probe(load_ptr, loopblock);
        IRB->CreateCall(carv_name_pop, empty_args);
      }

      Value * index_update_instr
        = IRB->CreateAdd(index_phi, ConstantInt::get(Int32Ty, 1));
      index_phi->addIncoming(index_update_instr, loopblock);

      Instruction * cmp_instr2
        = (Instruction *) IRB->CreateICmpSLT(index_update_instr, pointer_size);

      BasicBlock * endblock
        = loopblock->splitBasicBlock(cmp_instr2->getNextNonDebugInstruction());

      IRB->SetInsertPoint(casted_val->getNextNonDebugInstruction());

      Instruction * BB_term = IRB->CreateBr(loopblock_start);
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

    } else {
      Value * ptrval = val;
      if (val_type != Int8PtrTy) {
        ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, val, Int8PtrTy);
      }

      bool is_class_type = false;
      Value * default_class_idx = ConstantInt::get(Int32Ty, -1);
      if (pointee_type->isStructTy()) {
        StructType * struct_type = dyn_cast<StructType> (pointee_type);
        auto search = class_name_map.find(struct_type);
        if (search != class_name_map.end()) {
          is_class_type = true;
          default_class_idx = ConstantInt::get(Int32Ty, search->second.first);
        }
      }

      Value * pointee_size_val = ConstantInt::get(Int32Ty, pointee_size);

      std::string typestr = get_type_str(pointee_type);
      Constant * typestr_const = gen_new_string_constant(typestr, IRB);
      //Call Carv_pointer
      std::vector<Value *> probe_args {ptrval, typestr_const, default_class_idx, pointee_size_val};
      Value * end_size = IRB->CreateCall(carv_ptr_func, probe_args);
      
      Value * class_idx = NULL;
      if (is_class_type) {
        pointee_size_val = IRB->CreateCall(get_class_size, empty_args);
        class_idx = IRB->CreateCall(get_class_idx, empty_args);
      }

      Instruction * pointer_size = (Instruction *)
        IRB->CreateSDiv(end_size, pointee_size_val);

      //Make loop block
      BasicBlock * loopblock = BB->splitBasicBlock(pointer_size->getNextNonDebugInstruction());
      BasicBlock * const loopblock_start = loopblock;

      IRB->SetInsertPoint(pointer_size->getNextNonDebugInstruction());
      
      Instruction * cmp_instr1 = (Instruction *)
        IRB->CreateICmpEQ(pointer_size, ConstantInt::get(Int32Ty, 0));

      IRB->SetInsertPoint(loopblock->getFirstNonPHIOrDbgOrLifetime());
      PHINode * index_phi = IRB->CreatePHI(Int32Ty, 2);
      index_phi->addIncoming(ConstantInt::get(Int32Ty, 0), BB);

      std::vector<Value *> probe_args2 {index_phi};
      IRB->CreateCall(carv_ptr_name_update, probe_args2);

      if (is_class_type) {
        std::vector<Value *> args1 {ptrval, index_phi, pointee_size_val};
        Value * elem_ptr = IRB->CreateCall(update_class_ptr, args1);
        std::vector<Value *> args {elem_ptr, class_idx};
        IRB->CreateCall(class_carver, args);
        IRB->CreateCall(name_free_pop, empty_args);
      } else {
        Value * getelem_instr = IRB->CreateGEP(pointee_type, val, index_phi);

        if (pointee_type->isStructTy()) {
          insert_struct_carve_probe(getelem_instr, pointee_type);
          IRB->CreateCall(name_free_pop, empty_args);
        } else if (is_func_ptr_type(pointee_type)) {
          Value * load_ptr = IRB->CreateLoad(pointee_type, getelem_instr);
          Value * cast_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, load_ptr, Int8PtrTy);
          std::vector<Value *> probe_args {cast_ptr};
          IRB->CreateCall(carv_func_ptr, probe_args);
          IRB->CreateCall(carv_name_pop, empty_args);
        } else {
          Value * load_ptr = IRB->CreateLoad(pointee_type, getelem_instr);
          loopblock = insert_carve_probe(load_ptr, loopblock);
          IRB->CreateCall(carv_name_pop, empty_args);
        }
      } 

      Value * index_update_instr
        = IRB->CreateAdd(index_phi, ConstantInt::get(Int32Ty, 1));
      index_phi->addIncoming(index_update_instr, loopblock);

      Instruction * cmp_instr2
        = (Instruction *) IRB->CreateICmpSLT(index_update_instr, pointer_size);

      BasicBlock * endblock
        = loopblock->splitBasicBlock(cmp_instr2->getNextNonDebugInstruction());

      IRB->SetInsertPoint(pointer_size->getNextNonDebugInstruction());

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
  } else {
    DEBUG0("Unknown type input : \n");
    DEBUGDUMP(val);
  }

  return BB;
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

  std::string struct_carver_name = "__Carv_" + struct_name;
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
    bool found_DIType = false;
    std::vector<std::string> elem_names;
    for (auto iter : DbgFinder.types()) {
      if (struct_name == iter->getName().str()) {
        found_DIType = true;
        DIType * dit = iter;
        get_struct_field_names_from_DIT(dit, &elem_names);
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
                  DEBUGDUMP(DItype);
                  get_struct_field_names_from_DIT(DItype, &elem_names);
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
      DEBUG0("Warn : Wrong # of elem names....\n");
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
      Constant * field_name_const = gen_new_string_constant(iter, IRB);
      std::vector<Value *> struct_name_probe_args {field_name_const};
      IRB->CreateCall(struct_name_func, struct_name_probe_args);

      Value * gep = IRB->CreateStructGEP(struct_type, carver_param, elem_idx);
      PointerType* gep_type = dyn_cast<PointerType>(gep->getType());
      Type * gep_pointee_type = gep_type->getPointerElementType();

      if (gep_pointee_type->isStructTy()) {
        insert_struct_carve_probe(gep, gep_pointee_type);
        IRB->CreateCall(name_free_pop, empty_args);
      } else if (gep_pointee_type->isArrayTy()) {
        cur_block = insert_carve_probe(gep, cur_block);
        IRB->CreateCall(name_free_pop, empty_args);
      } else if (is_func_ptr_type(gep_pointee_type)) {
        Value * load_ptr = IRB->CreateLoad(gep_pointee_type, gep);
        Value * cast_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, load_ptr, Int8PtrTy);
        std::vector<Value *> probe_args {cast_ptr};
        IRB->CreateCall(carv_func_ptr, probe_args);
        IRB->CreateCall(name_free_pop, empty_args);
      } else {
        Value * loadval = IRB->CreateLoad(gep_pointee_type, gep);
        cur_block = insert_carve_probe(loadval, cur_block);
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

  StructType * struct_type = dyn_cast<StructType>(type);

  auto search2 = class_name_map.find(struct_type);
  if (search2 == class_name_map.end()) {
    insert_struct_carve_probe_inner(struct_ptr, type);
  } else {
    Value * casted
      = IRB->CreateCast(Instruction::CastOps::BitCast, struct_ptr, Int8PtrTy);
    std::vector<Value *> args { casted, ConstantInt::get(Int32Ty, search2->second.first)};
    IRB->CreateCall(class_carver, args);
  }

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
  Int1Ty = IntegerType::getInt1Ty(C);
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
    , Int32Ty, Int8PtrTy, Int8PtrTy, Int32Ty, Int32Ty);
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
    get_link_name("__keep_class_name"), VoidTy, Int8PtrTy, Int32Ty);
  get_class_idx = M.getOrInsertFunction(
    get_link_name("__get_class_idx"), Int32Ty);
  get_class_size = M.getOrInsertFunction(
    get_link_name("__get_class_size"), Int32Ty);
  update_class_ptr = M.getOrInsertFunction(
    get_link_name("__update_class_ptr"), Int8PtrTy, Int8PtrTy, Int32Ty, Int32Ty);

  get_class_type_info();

  get_instrument_func_set();

  find_global_var_uses();

  gen_class_carver();

  DEBUG0("Iterating functions...\n");

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) { continue; }
    if (&F == class_carver.getCallee()) { continue;}

    std::string func_name = F.getName().str();
    if (instrument_func_set.find(func_name) == instrument_func_set.end()) {
      std::vector<CallInst *> call_instrs;
      std::vector<ReturnInst *> ret_instrs;

      for (auto &BB : F) {
        for (auto &IN : BB) {
          if (isa<CallInst>(&IN)) {
            call_instrs.push_back(dyn_cast<CallInst>(&IN));
          } else if (isa<ReturnInst>(&IN)) {
            ret_instrs.push_back(dyn_cast<ReturnInst>(&IN));
          }
        }
      }

      BasicBlock& entry_block = F.getEntryBlock();
      Insert_alloca_probe(entry_block);

      //Just perform memory tracking
      for (auto call_instr : call_instrs) {
        Function * callee = call_instr->getCalledFunction();
        if ((callee == NULL) || (callee->isDebugInfoForProfiling())) { continue; }
        std::string callee_name = callee->getName().str();
        Insert_callinst_probe(call_instr, callee_name, false);
      }

      for (auto ret_instr : ret_instrs) {
        IRB->SetInsertPoint(ret_instr);

        //Remove alloca (local variable) memory tracking info.
        for (auto iter = tracking_allocas.begin();
          iter != tracking_allocas.end(); iter++) {
          AllocaInst * alloc_instr = *iter;

          Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, alloc_instr, Int8PtrTy);
          std::vector<Value *> args {casted_ptr};
          IRB->CreateCall(remove_probe, args);
        }
      }

      tracking_allocas.clear();
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

        Constant * param_name_const = gen_new_string_constant(param_name, IRB);
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
      Constant * func_name_const = gen_new_string_constant(func_name, IRB);

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

  read_probe_list("carver_probe_names.txt");
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

