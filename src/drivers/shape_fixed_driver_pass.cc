#include "carve_pass.hpp"
#include "utils.hpp"

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
  bool read_carved_input();

  std::pair<BasicBlock *, Value *> insert_replay_probe(Type *, BasicBlock *);

  std::set<std::string> struct_replayes;
  void insert_struct_replay_probe_inner(Value*, Type *);
  void insert_struct_replay_probe(Value*, Type *);

  void make_stub(Function * F);

  void gen_class_replay();

  std::set<BasicBlock *> replay_BBs;
  
  FunctionCallee __inputf_open;

  FunctionCallee replay_char_func;
  FunctionCallee replay_short_func;
  FunctionCallee replay_int_func;
  FunctionCallee replay_long_func;
  FunctionCallee replay_longlong_func;
  FunctionCallee replay_float_func;
  FunctionCallee replay_double_func;

  FunctionCallee replay_func_ptr;
  FunctionCallee record_func_ptr;

  FunctionCallee receive_carved_ptr;
  FunctionCallee receive_ptr_shape;
  FunctionCallee receive_func_ptr;

  FunctionCallee replay_ptr_func;
  FunctionCallee replay_ptr_alloc_size;
  FunctionCallee replay_ptr_class_index;
  FunctionCallee replay_ptr_class_size;
  FunctionCallee update_class_ptr;

  FunctionCallee keep_class_name;

  FunctionCallee __replay_fini;

  FunctionCallee class_replay;

  int func_id;
};

}  // namespace

char driver_pass::ID = 0;

bool driver_pass::hookInstrs(Module &M) {
  initialize_pass_contexts(M);

  get_llvm_types();

  __inputf_open = M.getOrInsertFunction(
    get_link_name("__driver_inputf_open"), VoidTy, Int8PtrPtrTy);

  replay_char_func = M.getOrInsertFunction(get_link_name("Replay_char")
    , Int8Ty);
  replay_short_func = M.getOrInsertFunction(get_link_name("Replay_short")
    , Int16Ty);
  replay_int_func = M.getOrInsertFunction(get_link_name("Replay_int")
    , Int32Ty);
  replay_long_func = M.getOrInsertFunction(get_link_name("Replay_longtype")
    , Int64Ty);
  replay_longlong_func = M.getOrInsertFunction(get_link_name("Replay_longlong")
    , Int128Ty);
  replay_float_func = M.getOrInsertFunction(get_link_name("Replay_float")
    , FloatTy);
  replay_double_func = M.getOrInsertFunction(get_link_name("Replay_double")
    , DoubleTy);
  replay_ptr_func = M.getOrInsertFunction(get_link_name("Replay_pointer")
    , Int8PtrTy, Int8PtrTy);
  replay_ptr_alloc_size = M.getOrInsertFunction(get_link_name("Replay_ptr_alloc_size")
    , Int32Ty);

  replay_ptr_class_index = M.getOrInsertFunction(
    get_link_name("Replay_ptr_class_index"), Int32Ty);
  replay_ptr_class_size = M.getOrInsertFunction(
    get_link_name("Replay_ptr_class_size"), Int32Ty);

  replay_func_ptr = M.getOrInsertFunction(get_link_name("Replay_func_ptr")
    , Int8PtrTy);

  record_func_ptr = M.getOrInsertFunction(get_link_name("__record_func_ptr"),
    VoidTy, Int8PtrTy, Int8PtrTy);

  receive_carved_ptr = M.getOrInsertFunction(
    get_link_name("__receive_carved_ptr"), VoidTy, Int32Ty, Int8PtrTy);

  receive_ptr_shape = M.getOrInsertFunction(
    get_link_name("__receive_ptr_shape"), VoidTy, Int32Ty, Int32Ty);

  receive_func_ptr = M.getOrInsertFunction(
    get_link_name("__receive_func_ptr"), VoidTy, Int8PtrTy);

  update_class_ptr = M.getOrInsertFunction(
    get_link_name("__update_class_ptr"), Int8PtrTy, Int8PtrTy, Int32Ty, Int32Ty);
  
  keep_class_name = M.getOrInsertFunction(
    get_link_name("__keep_class_name"), VoidTy, Int8PtrTy, Int32Ty);

  __replay_fini = M.getOrInsertFunction(get_link_name("__replay_fini"), VoidTy);


  bool res = get_target_func();
  if (res == false) {
    DEBUG0("get_target_func failed\n");
    return true;
  }

  get_class_type_info();

  gen_class_replay();

  find_global_var_uses();

  DEBUG0("Iterating functions...\n");

  for (auto &F : M) {
    if (F.isIntrinsic() || !F.size()) { continue; }
    std::string func_name = F.getName().str();

    if (func_name == "_GLOBAL__sub_I_") { continue;}
    if (func_name == "__cxx_global_var_init") { continue; }
    if (func_name == "__class_replay") { continue; }
    if (func_name.find("__Replay__") != std::string::npos) { continue; }

    if (func_name == "main") {
      main_func = &F;
    } else if (target_func != &F) {
      make_stub(&F);
    }
  }

  if (main_func == target_func) {
    DEBUG0("Unimplemented : main function can not be the target function.\n");
    return false;
  }

  BasicBlock * new_entry_block = BasicBlock::Create(*Context, "new_entry_block", main_func);

  BasicBlock * cur_block = new_entry_block;
  replay_BBs.insert(cur_block);

  IRB->SetInsertPoint(cur_block);

  //Record func ptr
  for (auto &Func : M.functions()) {
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

  Value * argv = main_func->getArg(1);
  std::vector<Value *> reader_args {argv};
  IRB->CreateCall(__inputf_open, reader_args);

  res = read_carved_input();
  if (res == false) {
    DEBUG0("read_carved_input failed\n"); 
    return true;
  }

  std::vector<Value *> target_args;
  for (auto &arg : target_func->args()) {
    Type * arg_type = arg.getType();
    auto replay_res = insert_replay_probe(arg_type, cur_block);
    cur_block = replay_res.first;
    target_args.push_back(replay_res.second);
  }

  auto search = global_var_uses.find(target_func);
  if (search != global_var_uses.end()) {
    for (auto glob_iter : search->second) {
      Type * val_type = glob_iter->getType();
      auto replay_res = insert_replay_probe(val_type, cur_block);
      cur_block = replay_res.first;
      
      Value * glob_val = replay_res.second;
      IRB->CreateStore(glob_val, glob_iter);
    }
  }

  Instruction * target_call = IRB->CreateCall(
    target_func->getFunctionType(), target_func, target_args);

  //Return
  IRB->CreateCall(__replay_fini, {});

  IRB->CreateRet(ConstantInt::get(Int32Ty, 0));

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

  char * tmp = getenv("DUMP_IR");
  if (tmp) {
    DEBUG0("Dumping IR...\n");
    DEBUGDUMP(Mod);
  }

  delete IRB;
  return true;
}

std::pair<BasicBlock *, Value *>
  driver_pass::insert_replay_probe (Type * typeptr, BasicBlock * BB) {
  std::vector<Value *> probe_args;
  BasicBlock * cur_block = BB;
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
  } else if (typeptr == FloatTy) {
    result = IRB->CreateCall(replay_float_func, {});
  } else if (typeptr == DoubleTy) {
    result = IRB->CreateCall(replay_double_func, {});
  } else if (typeptr->isStructTy()) {
    //TODO
  } else if (typeptr->isFunctionTy() || typeptr->isArrayTy()) {
    //Is it possible to reach here?
  } else if (typeptr->isPointerTy()) {
    PointerType * ptrtype = dyn_cast<PointerType>(typeptr);

    if (ptrtype->isOpaque() || ptrtype->isOpaquePointerTy()) {
      return std::make_pair(cur_block , result);
    }

    Type * pointee_type = ptrtype->getPointerElementType();

    unsigned pointee_size = DL->getTypeAllocSize(pointee_type);
    if (pointee_size == 0) { return std::make_pair(cur_block , result); }

    if (isa<StructType> (pointee_type)) {
      StructType * tmptype = dyn_cast<StructType>(pointee_type);
      if (tmptype->isOpaque()) { return std::make_pair(cur_block , result); }
    }

    if (pointee_type->isArrayTy()) {
      //TODO
    } else {
      std::vector<Value *> replay_ptr_func_args {};
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

      if (is_class_type) {
        replay_ptr_func_args.push_back(gen_new_string_constant(
          get_type_str(pointee_type), IRB));
      } else {
        replay_ptr_func_args.push_back(Constant::getNullValue(Int8PtrTy));
      }

      
      result = IRB->CreateCall(replay_ptr_func, replay_ptr_func_args);

      Value * casted_result = result;

      if (typeptr != Int8PtrTy) {
        casted_result = IRB->CreatePointerCast(result, typeptr);
      }

      Value * pointee_size_val = ConstantInt::get(Int32Ty, pointee_size);

      Value * class_idx = NULL;
      if (is_class_type) {
        pointee_size_val = IRB->CreateCall(replay_ptr_class_size, {});
        class_idx = IRB->CreateCall(replay_ptr_class_index, {});
      }
      
      Instruction * ptr_bytesize = IRB->CreateCall(replay_ptr_alloc_size, probe_args);
      Instruction * ptr_size = (Instruction *) IRB->CreateSDiv(ptr_bytesize
        , pointee_size_val);
      
      //Make loop block
      BasicBlock * loopblock = BasicBlock::Create(*Context, "loop", cur_block->getParent());
      BasicBlock * const loopblock_start = loopblock;
      replay_BBs.insert(loopblock);
      
      Instruction * cmp_instr1 = (Instruction *)
        IRB->CreateICmpEQ(ptr_size, ConstantInt::get(Int32Ty, 0));
      
      Instruction * temp_br_instr = IRB->CreateBr(loopblock);

      IRB->SetInsertPoint(loopblock);
      PHINode * index_phi = IRB->CreatePHI(Int32Ty, 2);
      index_phi->addIncoming(ConstantInt::get(Int32Ty, 0), cur_block);

      if (is_class_type) {
        std::vector<Value *> args1 {result, index_phi, pointee_size_val};
        Value * elem_ptr = IRB->CreateCall(update_class_ptr, args1);
        std::vector<Value *> args {elem_ptr, class_idx};
        IRB->CreateCall(class_replay, args);
      } else {
        Value * getelem_instr = IRB->CreateGEP(pointee_type, casted_result, index_phi);

        if (pointee_type->isStructTy()) {
          insert_struct_replay_probe(getelem_instr, pointee_type);
        }  else if (is_func_ptr_type(pointee_type)) {
          Value * func_ptr_val = IRB->CreateCall(replay_func_ptr, {});
          Value * casted_val = IRB->CreateBitCast(func_ptr_val, pointee_type);
          IRB->CreateStore(casted_val, getelem_instr);
        } else {
          auto ptr_result = insert_replay_probe(pointee_type, loopblock);
          loopblock = ptr_result.first;
          Value * ptr_replay_res = ptr_result.second;
          if (ptr_replay_res != NULL) {
            Value * casted_val = IRB->CreateBitCast(ptr_replay_res, pointee_type);
            IRB->CreateStore(casted_val, getelem_instr);
          }
        }
      }

      Value * index_update_instr
        = IRB->CreateAdd(index_phi, ConstantInt::get(Int32Ty, 1));
      index_phi->addIncoming(index_update_instr, loopblock);

      Instruction * cmp_instr2
        = (Instruction *) IRB->CreateICmpSLT(index_update_instr, ptr_size);
      
      Instruction * temp_br_instr2 = IRB->CreateBr(loopblock);

      BasicBlock * endblock = BasicBlock::Create(*Context, "end", cur_block->getParent());
      replay_BBs.insert(endblock);

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

      cur_block = endblock;

      result = casted_result;
    }
    
  } else {
    DEBUGDUMP(typeptr);
    DEBUG0("Unknown type\n");
  }



  return std::make_pair(cur_block , result);
}

void driver_pass::insert_struct_replay_probe_inner(Value * struct_ptr
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

      if (field_type->isStructTy()) {
        insert_struct_replay_probe(gep, field_type);
      } else if (is_func_ptr_type(field_type)) {
        Value * func_ptr_val = IRB->CreateCall(replay_func_ptr, {});
        Value * casted_val = IRB->CreateBitCast(func_ptr_val, field_type);
        IRB->CreateStore(casted_val, gep);
      } else {
        auto ptr_result = insert_replay_probe(field_type, cur_block);
        cur_block = ptr_result.first;
        Value * ptr_replay_res = ptr_result.second;
        if (ptr_replay_res != NULL) {
          Value * casted_val = IRB->CreateBitCast(ptr_replay_res, field_type);      
          IRB->CreateStore(casted_val, gep);
        }
      }
    }

    IRB->CreateRetVoid();
  }

  IRB->restoreIP(cur_ip);
  std::vector<Value *> replay_args {struct_ptr};
  IRB->CreateCall(struct_replay, replay_args);
} 

void driver_pass::insert_struct_replay_probe(Value * ptr, Type * typeptr) {
  StructType * struct_type = dyn_cast<StructType>(typeptr);

  auto search2 = class_name_map.find(struct_type);
  if (search2 == class_name_map.end()) {
    insert_struct_replay_probe_inner(ptr, typeptr);
  } else {
    Value * casted
      = IRB->CreateCast(Instruction::CastOps::BitCast, ptr, Int8PtrTy);
    std::vector<Value *> args { casted, ConstantInt::get(Int32Ty, search2->second.first)};
    IRB->CreateCall(class_replay, args);
  }

  return;
}

bool driver_pass::runOnModule(Module &M) {

  DEBUG0("Running driver_pass\n");

  read_probe_list("driver_probe_names.txt");
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

bool driver_pass::get_target_func() {
  std::ifstream funcnames("funcs.txt");

  if (funcnames.fail()) {
    DEBUG0("Failed to open funcs.txt\n");
    return false;
  }

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
    DEBUG0("Can't find target\n");
    DEBUG0("func_idx_env : " << func_idx_env << "\n");
    return false;
  }

  return true;
}

bool driver_pass::read_carved_input() {
  std::ifstream input_file("input");
  std::string line;

  if (input_file.fail()) {
    DEBUG0("Failed to open input file\n");
    return false;
  }

  std::string out_file_name
    = Mod->getName().str() + "." + target_name + ".init";
  FILE * scalar_file = fopen(out_file_name.c_str(), "wb");
  
  bool is_carved_ptr = true;
  while(std::getline(input_file, line)) {
    if (is_carved_ptr) {
      if (line[0] == '#') {
        is_carved_ptr = false;
      } else {
        auto colon_pos1 = line.find(':');
        if (colon_pos1 == std::string::npos) { return false; }
        auto colon_pos2 = line.find(':', colon_pos1 + 1);
        if (colon_pos2 == std::string::npos) { return false; }
        auto colon_pos3 = line.find(':', colon_pos2 + 1);
        if (colon_pos3 == std::string::npos) { return false; }
        std::string size_ptr = line.substr(colon_pos2 + 1
          , colon_pos3 - colon_pos2 - 1);
        std::string type_str = line.substr(colon_pos3 + 1);

        Value * size_val = ConstantInt::get(Int32Ty, std::stoi(size_ptr));
        Value * name_val = gen_new_string_constant(type_str, IRB);

        std::vector<Value * > args {size_val, name_val};
        IRB->CreateCall(receive_carved_ptr, args);
      }
    } else {
      auto colon_pos1 = line.find(':');
      if (colon_pos1 == std::string::npos) { return false; }
      auto colon_pos2 = line.find(':', colon_pos1 + 1);
      if (colon_pos2 == std::string::npos) { return false; }
      auto colon_pos3 = line.find(':', colon_pos2 + 1);
      std::string var_name = line.substr(0, colon_pos1);
      std::string typestr = line.substr(colon_pos1 + 1
        , colon_pos2 - colon_pos1 - 1);
      std::string value_str;
      if (colon_pos3 == std::string::npos) {
        value_str = line.substr(colon_pos2 + 1);
      } else {
        value_str = line.substr(colon_pos2 + 1
          , colon_pos3 - colon_pos2 - 1);
      }

      if (typestr == "CHAR") {
        char value = std::stoi(value_str);
        fwrite(&value, sizeof(char), 1, scalar_file);
      } else if (typestr == "SHORT") {
        short value = std::stoi(value_str);
        fwrite(&value, sizeof(short), 1, scalar_file);
      } else if (typestr == "INT") {
        int value = std::stoi(value_str);
        fwrite(&value, sizeof(int), 1, scalar_file);
      } else if (typestr == "LONG") {
        long value = std::stol(value_str);
        fwrite(&value, sizeof(long), 1, scalar_file);
      } else if (typestr == "LONGLONG") {
        long long value = std::stoll(value_str);
        fwrite(&value, sizeof(long long), 1, scalar_file);
      } else if (typestr == "FLOAT") {
        float value = std::stof(value_str);
        fwrite(&value, sizeof(float), 1, scalar_file);
      } else if (typestr == "DOUBLE") {
        double value = std::stod(value_str);
        fwrite(&value, sizeof(double), 1, scalar_file);
      } else if (typestr == "NULL") {
        std::vector<Value * > args {ConstantInt::get(Int32Ty, -1), ConstantInt::get(Int32Ty, -1)};
        IRB->CreateCall(receive_ptr_shape, args);
      } else if (typestr == "FUNCPTR") {
        Constant * func_name_const = gen_new_string_constant(value_str, IRB);
        std::vector<Value * > args {func_name_const};
        IRB->CreateCall(receive_func_ptr, args);
      } else if (typestr == "PTR") {
        if (colon_pos3 == std::string::npos) { return false; }
        std::string offset_str = line.substr(colon_pos3 + 1);

        VAR<int> inputv = VAR<int> (std::stoi(value_str)
          , strdup(var_name.c_str()), std::stoi(offset_str)
          , INPUT_TYPE::POINTER);

        Value * ptr_idx = ConstantInt::get(Int32Ty, std::stoi(value_str));
        Value * ptr_offset = ConstantInt::get(Int32Ty, std::stoi(offset_str));

        std::vector<Value * > args {ptr_idx, ptr_offset};
        IRB->CreateCall(receive_ptr_shape, args);
      }
    }
  }

  input_file.close();
  fclose(scalar_file);

  DEBUG0("Successfully read input\n");
  return true;
}

void driver_pass::make_stub(Function * F) {
  BasicBlock * entry_block = BasicBlock::Create(*Context, "entry", F);
  IRB->SetInsertPoint(entry_block);

  replay_BBs.insert(entry_block);

  Type * return_type = F->getReturnType();
  if (return_type == VoidTy) {
    IRB->CreateRetVoid();  
  } else {
    auto replay_res = insert_replay_probe(return_type, entry_block);
    IRB->CreateRet(replay_res.second);
  }

  std::vector<BasicBlock *> BBs;
  for (auto &BB : F->getBasicBlockList()) {
    BBs.push_back(&BB);
  }

  for (auto BB : BBs) {
    if (replay_BBs.find(BB) == replay_BBs.end()) {
      BB->eraseFromParent();
    }
  }
}

void driver_pass::gen_class_replay() {
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
    = IRB->CreateSwitch(class_idx, default_BB, num_class_name_const);

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

  return;
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

