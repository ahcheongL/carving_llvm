#include "driver_pass.hpp"

FunctionCallee __inputf_modifier;
FunctionCallee __inputf_open;

FunctionCallee replay_char_func;
FunctionCallee replay_short_func;
FunctionCallee replay_int_func;
FunctionCallee replay_long_func;
FunctionCallee replay_longlong_func;
FunctionCallee replay_float_func;
FunctionCallee replay_double_func;

FunctionCallee replay_ptr_func;

FunctionCallee replay_func_ptr;
FunctionCallee record_func_ptr;

FunctionCallee update_class_ptr;

FunctionCallee keep_class_info;

FunctionCallee __replay_fini;

FunctionCallee class_replay;

Constant * global_cur_class_index = NULL;
Constant * global_cur_class_size = NULL;
Constant * global_ptr_alloc_size = NULL;

void make_stub(Function * F) {
  std::vector<BasicBlock *> BBs;
  for (auto &BB : F->getBasicBlockList()) {
    BBs.push_back(&BB);
  }

  for (auto BB : BBs) {
    BB->dropAllReferences();
  }

  for (auto BB : BBs) {
    BB->eraseFromParent();
  }
  
  BasicBlock * entry_block = BasicBlock::Create(*Context, "entry", F, &F->getEntryBlock());
  IRB->SetInsertPoint(entry_block);

  Type * return_type = F->getReturnType();
  if (return_type == VoidTy) {
    IRB->CreateRetVoid();  
  } else {
    Value * replay_res = insert_replay_probe(return_type, NULL);
    if (replay_res == NULL) {
      if (return_type->isPointerTy()) {
        //return null
        PointerType * ptr_type = dyn_cast<PointerType>(return_type);
        IRB->CreateRet(ConstantPointerNull::get(ptr_type));
      } else if (return_type->isStructTy()) {
        StructType * struct_type = dyn_cast<StructType>(return_type);
        IRB->CreateRet(ConstantAggregateZero::get(struct_type));
      } else {
        DEBUG0("Returning unknown type\n");
        F->dump();
      }
    } else {
      IRB->CreateRet(replay_res);
    }
  }
}

Value * insert_replay_probe (Type * typeptr, Value * ptr) {
  Value * result = NULL;

  if (typeptr == Int1Ty) {
    result = IRB->CreateCall(replay_char_func, {});
    result = IRB->CreateCast(Instruction::CastOps::Trunc, result, Int1Ty);
  } else if (typeptr == Int8Ty) {
    result = IRB->CreateCall(replay_char_func, {});
  } else if (typeptr == Int16Ty) {
    result = IRB->CreateCall(replay_short_func, {});
  } else if (typeptr == Int32Ty) {
    result = IRB->CreateCall(replay_int_func, {});
  } else if (typeptr == Int64Ty) {
    result = IRB->CreateCall(replay_long_func, {});
  } else if (typeptr == Int128Ty) {
    result = IRB->CreateCall(replay_longlong_func, {});
  } else if (typeptr->isIntegerTy()) {
    result = IRB->CreateCall(replay_longlong_func, {});
    result = IRB->CreateCast(Instruction::CastOps::Trunc, result, typeptr);
  } else if (typeptr == FloatTy) {
    result = IRB->CreateCall(replay_float_func, {});
  } else if (typeptr == DoubleTy) {
    result = IRB->CreateCall(replay_double_func, {});
  } else if (typeptr->isStructTy()) {
    StructType * struct_type = dyn_cast<StructType>(typeptr);
    
    unsigned int num_elem = struct_type->getNumElements();

    unsigned int idx = 0;
    for (idx = 0; idx < num_elem; idx ++) {
      Type * field_type = struct_type->getElementType(idx);
      Value * carved_val = insert_replay_probe(field_type, NULL);
      if (carved_val == NULL) { return NULL; }
      result = IRB->CreateInsertValue(UndefValue::get(typeptr), carved_val, idx);
    }
  } else if (is_func_ptr_type(typeptr)) {
    result = IRB->CreateCall(replay_func_ptr, {});
    result = IRB->CreateBitCast(result, typeptr);
  } else if (typeptr->isFunctionTy()) {
    //Is it possible to reach here?
  } else if (typeptr->isArrayTy()) {
    //Is it possible to reach here?
  } else if (typeptr->isPointerTy()) {
    PointerType * ptrtype = dyn_cast<PointerType>(typeptr);

    if (ptrtype->isOpaque() || ptrtype->isOpaquePointerTy()) { return NULL; }

    Type * pointee_type = ptrtype->getPointerElementType();

    if (isa<StructType> (pointee_type)) {
      StructType * tmptype = dyn_cast<StructType>(pointee_type);
      if (tmptype->isOpaque()) { return NULL; }
    }

    unsigned pointee_size = DL->getTypeAllocSize(pointee_type);
    if (pointee_size == 0) { return NULL; }

    bool is_class_type = false;
    Value * default_class_idx = NULL;
    Constant * class_name_const = NULL;
    if (pointee_type->isStructTy()) {
      StructType * struct_type = dyn_cast<StructType> (pointee_type);
      auto search = class_name_map.find(struct_type);
      if (search != class_name_map.end()) {
        is_class_type = true;
        default_class_idx = ConstantInt::get(Int32Ty, search->second.first);
        std::string struct_name = struct_type->getName().str();
        class_name_const = gen_new_string_constant(struct_name, IRB);
      }
    } else if (pointee_type == Int8Ty) {
      is_class_type = true;
      default_class_idx = ConstantInt::get(Int32Ty, num_class_name_const);
      class_name_const = gen_new_string_constant("i8", IRB);
    }

    if (is_class_type) {
      result = IRB->CreateCall(replay_ptr_func
        , {default_class_idx, ConstantInt::get(Int32Ty, pointee_size)
          , class_name_const});
    } else {
      result = IRB->CreateCall(replay_ptr_func
        , {ConstantInt::get(Int32Ty, 0)
            , ConstantInt::get(Int32Ty, pointee_size)
            , ConstantPointerNull::get(Int8PtrTy)});
    }
    
    result = IRB->CreatePointerCast(result, typeptr);

    Value * pointee_size_val = ConstantInt::get(Int32Ty, pointee_size);

    Value * class_idx = NULL;
    if (is_class_type) {
      pointee_size_val = IRB->CreateLoad(Int32Ty, global_cur_class_size);
      class_idx = IRB->CreateLoad(Int32Ty, global_cur_class_index);
    }
    
    Value * ptr_bytesize = IRB->CreateLoad(Int32Ty, global_ptr_alloc_size);
    Value * ptr_size = IRB->CreateSDiv(ptr_bytesize, pointee_size_val);

    BasicBlock * start_block = IRB->GetInsertBlock();

    Function * cur_func = start_block->getParent();
    
    //Make loop block
    BasicBlock * loopblock = BasicBlock::Create(*Context, "loop", cur_func, start_block->getNextNode());
    BasicBlock * const loopblock_start = loopblock;
    
    Value * cmp_instr1 = IRB->CreateICmpEQ(ptr_size
      , ConstantInt::get(Int32Ty, 0));
    
    Instruction * temp_br_instr = IRB->CreateBr(loopblock);

    IRB->SetInsertPoint(loopblock);
    PHINode * index_phi = IRB->CreatePHI(Int32Ty, 2);
    index_phi->addIncoming(ConstantInt::get(Int32Ty, 0), start_block);

    if (is_class_type) {
      Value * casted_result = IRB->CreateBitCast(result, Int8PtrTy);
      Value * elem_ptr = IRB->CreateCall(update_class_ptr
        , {casted_result, index_phi, pointee_size_val});
      IRB->CreateCall(class_replay, {elem_ptr, class_idx});
    } else {
      Value * getelem_instr = IRB->CreateGEP(pointee_type, result, index_phi);
      insert_gep_replay_probe(getelem_instr);
    }

    Value * index_update_instr
      = IRB->CreateAdd(index_phi, ConstantInt::get(Int32Ty, 1));
    index_phi->addIncoming(index_update_instr, IRB->GetInsertBlock());

    Value * cmp_instr2 = IRB->CreateICmpSLT(index_update_instr, ptr_size);
    
    Instruction * temp_br_instr2 = IRB->CreateBr(loopblock);

    BasicBlock * endblock = BasicBlock::Create(*Context, "end", cur_func, loopblock->getNextNode());

    IRB->SetInsertPoint(temp_br_instr);

    Instruction * BB_term
      = IRB->CreateCondBr(cmp_instr1, endblock, loopblock_start);
    BB_term->removeFromParent();
    ReplaceInstWithInst(temp_br_instr, BB_term);
    
    IRB->SetInsertPoint(temp_br_instr2);

    Instruction * loopblock_term
      = IRB->CreateCondBr(cmp_instr2, loopblock_start, endblock);
    loopblock_term->removeFromParent();
    ReplaceInstWithInst(temp_br_instr2, loopblock_term);

    IRB->SetInsertPoint(endblock);

  } else if(typeptr->isX86_FP80Ty()) {
    result = IRB->CreateCall(replay_double_func, {});
    result = IRB->CreateFPCast(result, typeptr);
  } else {
    DEBUGDUMP(typeptr);
    DEBUG0("Warning : Unknown type\n");
  }
  return result;
}


void insert_gep_replay_probe(Value * gep_val) {
  PointerType * gep_type = dyn_cast<PointerType>(gep_val->getType());
  Type * gep_pointee_type = gep_type->getPointerElementType();

  if (gep_pointee_type->isStructTy()) {
    insert_struct_replay_probe(gep_val, gep_pointee_type);
  } else if (is_func_ptr_type(gep_pointee_type)) {
    Value * func_ptr_val = IRB->CreateCall(replay_func_ptr, {});
    Value * casted_val = IRB->CreateBitCast(func_ptr_val, gep_pointee_type);
    IRB->CreateStore(casted_val, gep_val);
  } else if (gep_pointee_type->isArrayTy()) {
    ArrayType * array_type = dyn_cast<ArrayType>(gep_pointee_type);
    Type * array_elem_type = array_type->getArrayElementType();

    unsigned int array_size = array_type->getNumElements();
    unsigned int elem_size = DL->getTypeAllocSize(array_elem_type);
    int idx = 0;
    for (idx = 0; idx < array_size; idx++) {
      Value * ptr_result = insert_replay_probe(array_elem_type, NULL);
      if (ptr_result != NULL) {
        Value * array_gep = IRB->CreateInBoundsGEP(array_type, gep_val, {ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, idx)});
       IRB->CreateStore(ptr_result, array_gep);
      }
    }
  } else {
    Value * ptr_result = insert_replay_probe(gep_pointee_type, NULL);
    if (ptr_result != NULL) {
      Value * casted_val = IRB->CreateBitCast(ptr_result, gep_pointee_type);      
      IRB->CreateStore(casted_val, gep_val);
    }
  }

  return;
}


std::set<std::string> struct_replayes;
void insert_struct_replay_probe_inner(Value * struct_ptr
  , Type * type) {

  StructType * struct_type = dyn_cast<StructType>(type);

  IRBuilderBase::InsertPoint cur_ip = IRB->saveIP();

  std::string struct_name = struct_type->getName().str();
  struct_name = struct_name.substr(struct_name.find('.') + 1);
  if (struct_name.find("::") != std::string::npos) {
    struct_name = struct_name.substr(struct_name.find("::") + 2);
  }

  std::string struct_replay_name = "__Replay__" + struct_name;
  auto search = struct_replayes.find(struct_replay_name);
  FunctionCallee struct_replay = Mod->getOrInsertFunction(struct_replay_name
    , VoidTy, struct_ptr->getType());

  if (search == struct_replayes.end()) {
    struct_replayes.insert(struct_replay_name);

    //Define struct carver
    Function * struct_replay_func = dyn_cast<Function>(struct_replay.getCallee());

    BasicBlock * entry_BB
      = BasicBlock::Create(*Context, "entry", struct_replay_func);

    IRB->SetInsertPoint(entry_BB);

    BasicBlock * cur_block = entry_BB;
    Value * replay_param = struct_replay_func->getArg(0);

    unsigned num_fields = struct_type->getNumElements();

    for (unsigned elem_idx = 0; elem_idx < num_fields; elem_idx++) {
      Value * gep = IRB->CreateStructGEP(struct_type, replay_param, elem_idx);
      Type * field_type = struct_type->getElementType(elem_idx);
      insert_gep_replay_probe(gep);
    }

    IRB->CreateRetVoid();
  }

  IRB->restoreIP(cur_ip);
  IRB->CreateCall(struct_replay, {struct_ptr});
} 

void insert_struct_replay_probe(Value * ptr, Type * typeptr) {
  StructType * struct_type = dyn_cast<StructType>(typeptr);

  auto search2 = class_name_map.find(struct_type);
  if (search2 == class_name_map.end()) {
    insert_struct_replay_probe_inner(ptr, typeptr);
  } else {
    Value * casted
      = IRB->CreateCast(Instruction::CastOps::BitCast, ptr, Int8PtrTy);
    IRB->CreateCall(class_replay, {casted
      , ConstantInt::get(Int32Ty, search2->second.first)});
  }

  return;
}

void gen_class_replay() {
  class_replay
    = Mod->getOrInsertFunction("__class_replay", VoidTy, Int8PtrTy, Int32Ty);
  Function * class_replay_func = dyn_cast<Function>(class_replay.getCallee());
  
  BasicBlock * entry_BB
    = BasicBlock::Create(*Context, "entry", class_replay_func);

  BasicBlock * default_BB
    = BasicBlock::Create(*Context, "default", class_replay_func);
  IRB->SetInsertPoint(default_BB);
  IRB->CreateRetVoid();

  IRB->SetInsertPoint(entry_BB);

  Value * replaying_ptr = class_replay_func->getArg(0);
  Value * class_idx = class_replay_func->getArg(1);

  SwitchInst * switch_inst
    = IRB->CreateSwitch(class_idx, default_BB, num_class_name_const + 1);

  for (auto class_type : class_name_map) {
    int case_id = class_type.second.first;
    BasicBlock * case_block = BasicBlock::Create(*Context, std::to_string(case_id), class_replay_func);
    switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
    IRB->SetInsertPoint(case_block);

    StructType * class_type_ptr = class_type.first;
    
    Value * casted_var= IRB->CreateCast(Instruction::CastOps::BitCast
      , replaying_ptr, PointerType::get(class_type_ptr, 0));

    insert_struct_replay_probe_inner(casted_var, class_type_ptr);
    IRB->CreateRetVoid();
  }

  //char * type
  int case_id = num_class_name_const;
  BasicBlock * case_block = BasicBlock::Create(*Context, std::to_string(case_id), class_replay_func);
  switch_inst->addCase(ConstantInt::get(Int32Ty, case_id), case_block);
  IRB->SetInsertPoint(case_block);

  Value * new_value = IRB->CreateCall(replay_char_func, {});

  IRB->CreateStore(new_value, replaying_ptr);
  IRB->CreateRetVoid();

  switch_inst->setDefaultDest(case_block);
  default_BB->eraseFromParent();

  return;
}

void get_driver_func_callees() {
  __inputf_modifier = Mod->getOrInsertFunction(
    get_link_name("__driver_input_modifier"), VoidTy, Int8PtrTy, Int8PtrPtrPtrTy);
  __inputf_open = Mod->getOrInsertFunction(
    get_link_name("__driver_inputf_open"), VoidTy, Int8PtrTy);

  replay_char_func = Mod->getOrInsertFunction(get_link_name("Replay_char")
    , Int8Ty);
  replay_short_func = Mod->getOrInsertFunction(get_link_name("Replay_short")
    , Int16Ty);
  replay_int_func = Mod->getOrInsertFunction(get_link_name("Replay_int")
    , Int32Ty);
  replay_long_func = Mod->getOrInsertFunction(get_link_name("Replay_longtype")
    , Int64Ty);
  replay_longlong_func = Mod->getOrInsertFunction(get_link_name("Replay_longlong")
    , Int128Ty);
  replay_float_func = Mod->getOrInsertFunction(get_link_name("Replay_float")
    , FloatTy);
  replay_double_func = Mod->getOrInsertFunction(get_link_name("Replay_double")
    , DoubleTy);
  replay_ptr_func = Mod->getOrInsertFunction(get_link_name("Replay_pointer")
    , Int8PtrTy, Int32Ty, Int32Ty, Int8PtrTy);

  replay_func_ptr = Mod->getOrInsertFunction(get_link_name("Replay_func_ptr")
    , Int8PtrTy);

  record_func_ptr = Mod->getOrInsertFunction(get_link_name("__record_func_ptr"),
    VoidTy, Int8PtrTy, Int8PtrTy);

  keep_class_info = Mod->getOrInsertFunction(get_link_name("__keep_class_info"),
    VoidTy, Int8PtrTy, Int32Ty, Int32Ty);

  update_class_ptr = Mod->getOrInsertFunction(get_link_name("__update_class_ptr"),
    Int8PtrTy, Int8PtrTy, Int32Ty, Int32Ty);

  __replay_fini = Mod->getOrInsertFunction(get_link_name("__replay_fini"), VoidTy);

  global_cur_class_index = Mod->getOrInsertGlobal("__replay_cur_class_index", Int32Ty);
  global_cur_class_size = Mod->getOrInsertGlobal("__replay_cur_pointee_size", Int32Ty);
  global_ptr_alloc_size = Mod->getOrInsertGlobal("__replay_cur_alloc_size", Int32Ty);

}