
#include "pass.hpp"

Module * Mod;
LLVMContext * Context;
const DataLayout * DL;
IRBuilder<> *IRB;
DebugInfoFinder DbgFinder;

void initialize_pass_contexts(Module &M) {
  LLVMContext &              C = M.getContext();
  const DataLayout & dataLayout = M.getDataLayout();
  Mod = &M;
  Context = &C;
  DL = &dataLayout;
  IRB = new IRBuilder<> (C);
  
  DbgFinder.processModule(M);
}

static std::map<std::string, Constant *> new_string_globals;
static std::map<std::string, std::string> probe_link_names;

std::string get_type_str(Type * type) {
  std::string typestr;
  raw_string_ostream typestr_stream(typestr);
  type->print(typestr_stream);
  return typestr;
}

bool is_func_ptr_type(Type * type) {
  if (type->isPointerTy()) {
    PointerType * ptrtype = dyn_cast<PointerType>(type);
    Type * elem_type = ptrtype->getPointerElementType();
    return elem_type->isFunctionTy();
  }
  return false;
}

std::string get_link_name(std::string base_name) {
  
  auto search = probe_link_names.find(base_name);
  if (search == probe_link_names.end()) {
    DEBUG0("Can't find probe name : " << base_name << "! Abort.\n");
    std::abort();
  }

  return search->second;
}

void read_probe_list(std::string filename) {
  std::string file_path = __FILE__;
  file_path = file_path.substr(0, file_path.rfind("/"));
  file_path = file_path.substr(0, file_path.rfind("/"));

  std::string probe_file_path
    = file_path.substr(0, file_path.rfind("/")) + "/lib/" + filename;
  
  std::ifstream list_file(probe_file_path);

  std::string line;
  while(std::getline(list_file, line)) {
    size_t space_loc = line.find(' ');
    probe_link_names.insert(std::make_pair(line.substr(0, space_loc)
      , line.substr(space_loc + 1)));
  }

  if (probe_link_names.size() == 0) {
    DEBUG0("Can't find lib/" << filename << " file!\n");
    std::abort();
  }
}

Constant * gen_new_string_constant(std::string name, IRBuilder<> * IRB) {

  auto search = new_string_globals.find(name);

  if (search == new_string_globals.end()) {
    Constant * new_global = IRB->CreateGlobalStringPtr(name, "", 0, Mod);
    new_string_globals.insert(std::make_pair(name, new_global));
    return new_global;
  }

  return search->second;
}

std::string find_param_name(Value * param, BasicBlock * BB) {

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

void get_struct_field_names_from_DIT(DIType * dit, std::vector<std::string> * elem_names) {

  while ((dit != NULL) && isa<DIDerivedType>(dit)) {
    DIDerivedType * tmptype = dyn_cast<DIDerivedType>(dit);
    dit = tmptype->getBaseType();
  }

  if (dit == NULL) { return; }

  if (isa<DISubroutineType>(dit)) {
    DISubroutineType * subroutine_type = dyn_cast<DISubroutineType>(dit);
    for (auto subtype : subroutine_type->getTypeArray()) {
      if (subtype == NULL) { continue; }

      get_struct_field_names_from_DIT(subtype, elem_names);
    }
    
  } else if (isa<DICompositeType>(dit)) {
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
  } else {
    //TODO
    return;
  }
}

int num_class_name_const = 0;
std::vector<std::pair<Constant *, int>> class_name_consts;
std::map<StructType *, std::pair<int, Constant *>> class_name_map;

void get_class_type_info() {
  for (auto struct_type : Mod->getIdentifiedStructTypes()) {
    if (struct_type->isOpaque()) { continue; }
    //std::string name = get_type_str(struct_type);
    std::string name = struct_type->getName().str();
    Constant * name_const = gen_new_string_constant(name, IRB);    
    class_name_consts.push_back(std::make_pair(name_const, DL->getTypeAllocSize(struct_type)));
    class_name_map.insert(std::make_pair(struct_type
      , std::make_pair(num_class_name_const++, name_const)));
  }
}

std::map<Function *, std::set<Constant *>> global_var_uses;
void find_global_var_uses() {
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

void get_llvm_types() {
  VoidTy = Type::getVoidTy(*Context);
  Int1Ty = Type::getInt1Ty(*Context);
  Int8Ty = Type::getInt8Ty(*Context);
  Int16Ty = Type::getInt16Ty(*Context);
  Int32Ty = Type::getInt32Ty(*Context);
  Int64Ty = Type::getInt64Ty(*Context);
  Int128Ty = Type::getInt128Ty(*Context);
  FloatTy = Type::getFloatTy(*Context);
  DoubleTy = Type::getDoubleTy(*Context);
  Int8PtrTy = PointerType::get(Int8Ty, 0);
  Int16PtrTy = PointerType::get(Int16Ty, 0);
  Int32PtrTy = PointerType::get(Int32Ty, 0);
  Int64PtrTy = PointerType::get(Int64Ty, 0);
  Int128PtrTy = PointerType::get(Int128Ty, 0);
  Int8PtrPtrTy = PointerType::get(Int8PtrTy, 0);
  Int8PtrPtrPtrTy = PointerType::get(Int8PtrPtrTy, 0);
  FloatPtrTy = PointerType::get(FloatTy, 0);
  DoublePtrTy = PointerType::get(DoubleTy, 0);
}

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
FunctionCallee keep_class_name;
FunctionCallee get_class_idx;
FunctionCallee get_class_size;
FunctionCallee class_carver;
FunctionCallee update_class_ptr;

void get_carving_func_callees() {
  mem_allocated_probe = Mod->getOrInsertFunction(
    get_link_name("__mem_allocated_probe"), VoidTy, Int8PtrTy, Int32Ty);
  remove_probe = Mod->getOrInsertFunction(get_link_name("__remove_mem_allocated_probe")
    , VoidTy, Int8PtrTy);
  record_func_ptr = Mod->getOrInsertFunction(get_link_name("__record_func_ptr"),
    VoidTy, Int8PtrTy, Int8PtrTy);
  argv_modifier = Mod->getOrInsertFunction(get_link_name("__carver_argv_modifier")
    , VoidTy, Int32PtrTy, Int8PtrPtrPtrTy);
  __carv_fini = Mod->getOrInsertFunction(get_link_name("__carv_FINI")
    , VoidTy);
  strlen_callee = Mod->getOrInsertFunction("strlen", Int64Ty, Int8PtrTy);
  carv_char_func = Mod->getOrInsertFunction(get_link_name("Carv_char")
    , VoidTy, Int8Ty);
  carv_short_func = Mod->getOrInsertFunction(get_link_name("Carv_short")
    , VoidTy, Int16Ty);
  carv_int_func = Mod->getOrInsertFunction(get_link_name("Carv_int")
    , VoidTy, Int32Ty);
  carv_long_func = Mod->getOrInsertFunction(get_link_name("Carv_longtype")
    , VoidTy, Int64Ty);
  carv_longlong_func = Mod->getOrInsertFunction(get_link_name("Carv_longlong")
    , VoidTy, Int128Ty);
  carv_float_func = Mod->getOrInsertFunction(get_link_name("Carv_float")
    , VoidTy, FloatTy);
  carv_double_func = Mod->getOrInsertFunction(get_link_name("Carv_double")
    , VoidTy, DoubleTy);
  carv_ptr_func = Mod->getOrInsertFunction(get_link_name("Carv_pointer")
    , Int32Ty, Int8PtrTy, Int8PtrTy, Int32Ty, Int32Ty);
  carv_ptr_name_update = Mod->getOrInsertFunction(
    get_link_name("__carv_ptr_name_update"), VoidTy, Int32Ty);
  struct_name_func = Mod->getOrInsertFunction(
    get_link_name("__carv_struct_name_update"), VoidTy, Int8PtrTy);
  carv_name_push = Mod->getOrInsertFunction(
    get_link_name("__carv_name_push"), VoidTy, Int8PtrTy);
  carv_name_pop = Mod->getOrInsertFunction(
    get_link_name("__carv_name_pop"), VoidTy);
  carv_func_ptr = Mod->getOrInsertFunction(get_link_name("__Carv_func_ptr")
    , VoidTy, Int8PtrTy);
  carv_func_call = Mod->getOrInsertFunction(
    get_link_name("__carv_func_call_probe"), VoidTy, Int32Ty);
  carv_func_ret = Mod->getOrInsertFunction(
    get_link_name("__carv_func_ret_probe"), VoidTy, Int8PtrTy, Int32Ty);
  update_carved_ptr_idx = Mod->getOrInsertFunction(
    get_link_name("__update_carved_ptr_idx"), VoidTy); 
  mem_alloc_type = Mod->getOrInsertFunction(
    get_link_name("__mem_alloc_type"), VoidTy, Int8PtrTy, Int8PtrTy);
  keep_class_name = Mod->getOrInsertFunction(
    get_link_name("__keep_class_name"), VoidTy, Int8PtrTy, Int32Ty);
  get_class_idx = Mod->getOrInsertFunction(
    get_link_name("__get_class_idx"), Int32Ty);
  get_class_size = Mod->getOrInsertFunction(
    get_link_name("__get_class_size"), Int32Ty);
  update_class_ptr = Mod->getOrInsertFunction(
    get_link_name("__update_class_ptr"), Int8PtrTy, Int8PtrTy, Int32Ty, Int32Ty);
}

std::vector<AllocaInst *> tracking_allocas;

//Insert probes to track alloca instrution memory allcation
void Insert_alloca_probe(BasicBlock& entry_block) {
  
  std::vector<AllocaInst * > allocas;
  AllocaInst * alloca_instr = NULL;
  
  for (auto &IN : entry_block) {
    if (isa<AllocaInst>(&IN)) {
      alloca_instr = dyn_cast<AllocaInst>(&IN);
      allocas.push_back(alloca_instr);
    }
  }

  if (alloca_instr != NULL) {
    IRB->SetInsertPoint(alloca_instr->getNextNonDebugInstruction());
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
        std::string typestr = allocated_type->getStructName().str();       
        Constant * typename_const = gen_new_string_constant(typestr, IRB);
        IRB->CreateCall(mem_alloc_type, {casted_ptr, typename_const});
      }

      Value * size_const = ConstantInt::get(Int32Ty, size);
      std::vector<Value *> args {casted_ptr, size_const};
      IRB->CreateCall(mem_allocated_probe, args);
      tracking_allocas.push_back(alloc_instr);
    }
  }
}

static void insert_mem_alloc_type(Instruction * IN) {
  if (IN == NULL) { return; }

  CastInst * cast_instr;
  if ((cast_instr = dyn_cast<CastInst>(IN->getNextNonDebugInstruction()))) {
    Type * cast_type = cast_instr->getType();
    if (isa<PointerType>(cast_type)) {
      PointerType * cast_ptr_type = dyn_cast<PointerType>(cast_type);
      Type * pointee_type = cast_ptr_type->getPointerElementType();
      if (pointee_type->isStructTy()) {
        std::string typestr = pointee_type->getStructName().str();
        Constant * typename_const = gen_new_string_constant(typestr, IRB);
        IRB->CreateCall(mem_alloc_type, {IN, typename_const});
      }
    }
  }
}

void Insert_mem_func_call_probe(Instruction * IN, std::string callee_name) {
  IRB->SetInsertPoint(IN->getNextNonDebugInstruction());
  
  if (callee_name == "malloc") {
    //Track malloc
    insert_mem_alloc_type(IN);

    Value * size = IN->getOperand(0);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args {IN, size};
    IRB->CreateCall(mem_allocated_probe, args);
  } else if (callee_name == "realloc") {
    //Track realloc
    insert_mem_alloc_type(IN);

    std::vector<Value *> args0 {IN->getOperand(0)};
    IRB->CreateCall(remove_probe, args0);
    Value * size = IN->getOperand(1);
    if (size->getType() == Int64Ty) {
      size = IRB->CreateCast(Instruction::CastOps::Trunc, size, Int32Ty);
    }
    std::vector<Value *> args1 {IN, size};
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
    insert_mem_alloc_type(IN);

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
  }

  return;
}

unsigned int num_global_tracked = 0;
unsigned int num_func_tracked = 0;

void Insert_carving_main_probe(BasicBlock & entry_block, Function & F
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
    DEBUG0("This pass requires a main function" 
     << " which has argc, argv arguments\n");
    std::abort();
  }

  //Global variables memory probing
  for (auto global_iter = globals.begin(); global_iter != globals.end(); global_iter++) {

    if (!isa<GlobalVariable>(*global_iter)) { continue; }
    
    GlobalVariable * global_v = dyn_cast<GlobalVariable>(&(*global_iter));
    if (global_v->getName().str().find("llvm.") != std::string::npos) { continue; }
    
    Value * casted_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, &(*global_iter), Int8PtrTy);
    Type * gv_type = (*global_iter).getValueType();
    unsigned int size = 8;
    if (isa<GlobalVariable>(*global_iter)) {
      size = DL->getTypeAllocSize(gv_type);
    }
    Value * size_const = ConstantInt::get(Int32Ty, size);
    std::vector<Value *> args{casted_ptr, size_const};
    IRB->CreateCall(mem_allocated_probe, args);
    num_global_tracked++;
  }

  //Record func ptr
  for (auto &Func : Mod->functions()) {
    if (Func.size() == 0) { continue; }
    Constant * func_name_const = gen_new_string_constant(Func.getName().str(), IRB);
    Value * cast_val = IRB->CreateCast(Instruction::CastOps::BitCast
      , (Value *) &Func, Int8PtrTy);
    std::vector<Value *> probe_args {cast_val, func_name_const};
    IRB->CreateCall(record_func_ptr, probe_args);
    num_func_tracked++;
  }

  //Record class type string constants
  for (auto iter : class_name_consts) {
    IRB->CreateCall(keep_class_name
      , {iter.first, ConstantInt::get(Int32Ty, iter.second)});
  }

  Constant * ready = Mod->getOrInsertGlobal(get_link_name("__carv_ready"), Int8Ty);
  IRB->CreateStore(ConstantInt::get(Int8Ty, 1), ready);

  return;
}

BasicBlock * insert_carve_probe(Value * val, BasicBlock * BB) {
  Type * val_type = val->getType();

  if (val_type == Int1Ty) {
    Value * cast_val = IRB->CreateZExt(val, Int8Ty);
    IRB->CreateCall(carv_char_func, {cast_val});
  } else if (val_type == Int8Ty) {
    IRB->CreateCall(carv_char_func, {val});
  } else if (val_type == Int16Ty) {
    IRB->CreateCall(carv_short_func, {val});
  } else if (val_type == Int32Ty) {
    IRB->CreateCall(carv_int_func, {val});
  } else if (val_type == Int64Ty) {
    IRB->CreateCall(carv_long_func, {val});
  } else if (val_type == Int128Ty) {
    IRB->CreateCall(carv_longlong_func, {val});
  } else if (val_type == FloatTy) {
    IRB->CreateCall(carv_float_func, {val});
  } else if (val_type == DoubleTy) {
    IRB->CreateCall(carv_double_func, {val});
  } else if (val_type->isX86_FP80Ty()) {
    Value * cast_val = IRB->CreateFPCast(val, DoubleTy);
    IRB->CreateCall(carv_double_func, {cast_val});
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
      IRB->CreateCall(carv_name_pop, {});
      idx++;
    }

    return cur_block;
  } else if (is_func_ptr_type(val_type)) {
    Value * ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, val, Int8PtrTy);
    std::vector<Value *> probe_args {ptrval};
    IRB->CreateCall(carv_func_ptr, probe_args);
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

    unsigned int pointee_size = DL->getTypeAllocSize(pointee_type);
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
        IRB->CreateCall(carv_name_pop, {});
      } else if (is_func_ptr_type(array_elem_type)) {
        Value * ptrval = IRB->CreateCast(Instruction::CastOps::BitCast, casted_val, Int8PtrTy);
        std::vector<Value *> probe_args {ptrval};
        IRB->CreateCall(carv_func_ptr, probe_args);
        IRB->CreateCall(carv_name_pop, {});
      } else {
        Value * load_ptr = IRB->CreateLoad(array_elem_type, getelem_instr);
        loopblock = insert_carve_probe(load_ptr, loopblock);
        IRB->CreateCall(carv_name_pop, {});
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
      } else if (pointee_type == Int8Ty) {
        //check
        is_class_type = true;
        Value * default_class_idx = ConstantInt::get(Int32Ty, num_class_name_const);
      }

      Value * pointee_size_val = ConstantInt::get(Int32Ty, pointee_size);

      std::string typestr = get_type_str(pointee_type);
      Constant * typestr_const = gen_new_string_constant(typestr, IRB);
      //Call Carv_pointer
      std::vector<Value *> probe_args {ptrval, typestr_const, default_class_idx, pointee_size_val};
      Value * end_size = IRB->CreateCall(carv_ptr_func, probe_args);
      
      Value * class_idx = NULL;
      if (is_class_type) {
        pointee_size_val = IRB->CreateCall(get_class_size, {});
        class_idx = IRB->CreateCall(get_class_idx, {});
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
        IRB->CreateCall(carv_name_pop, {});
      } else {
        Value * getelem_instr = IRB->CreateGEP(pointee_type, val, index_phi);

        if (pointee_type->isStructTy()) {
          insert_struct_carve_probe(getelem_instr, pointee_type);
          IRB->CreateCall(carv_name_pop, {});
        } else if (is_func_ptr_type(pointee_type)) {
          Value * load_ptr = IRB->CreateLoad(pointee_type, getelem_instr);
          Value * cast_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, load_ptr, Int8PtrTy);
          std::vector<Value *> probe_args {cast_ptr};
          IRB->CreateCall(carv_func_ptr, probe_args);
          IRB->CreateCall(carv_name_pop, {});
        } else {
          Value * load_ptr = IRB->CreateLoad(pointee_type, getelem_instr);
          loopblock = insert_carve_probe(load_ptr, loopblock);
          IRB->CreateCall(carv_name_pop, {});
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

std::set<std::string> struct_carvers;
void insert_struct_carve_probe(Value * struct_ptr, Type * type) {

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

static std::map<std::string, DIType *> struct_ditype_map;

void construct_ditype_map() {
  for (auto iter : DbgFinder.types()) {
    std::string type_name = iter->getName().str();
    DIType * dit = iter;
    struct_ditype_map[type_name] = dit;
  }

  for (auto DIsubprog : DbgFinder.subprograms()) {
    std::string type_name = DIsubprog->getName().str();
    DIType * DISubroutType = DIsubprog->getType();
    struct_ditype_map[type_name] = DISubroutType;
  }
}

void insert_struct_carve_probe_inner(Value * struct_ptr, Type * type) {
  
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
    Instruction * ret_void_instr = IRB->CreateRetVoid();
    IRB->SetInsertPoint(entry_BB->getFirstNonPHIOrDbgOrLifetime());

    //Get field names
    std::vector<std::string> elem_names;

    auto search = struct_ditype_map.find(struct_name);
    if (search != struct_ditype_map.end()) {
      DIType * dit = search->second;
      get_struct_field_names_from_DIT(dit, &elem_names);
    }

    auto memberoffsets = SL->getMemberOffsets();

    if (elem_names.size() > memberoffsets.size()) {
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

    //depth check
    Constant * depth_check_const = Mod->getOrInsertGlobal(get_link_name("__carv_depth"), Int8Ty);

    Value * depth_check_val = IRB->CreateLoad(Int8Ty, depth_check_const);
    Value * depth_check_cmp = IRB->CreateICmpSGT(depth_check_val, ConstantInt::get(Int8Ty, STRUCT_CARV_DEPTH));
    BasicBlock * depth_check_BB = BasicBlock::Create(*Context, "depth_check", struct_carv_func);
    BasicBlock * do_carving_BB = BasicBlock::Create(*Context, "do_carving", struct_carv_func);
    BranchInst * depth_check_br = IRB->CreateCondBr(depth_check_cmp, depth_check_BB, do_carving_BB);
    depth_check_br->removeFromParent();
    ReplaceInstWithInst(ret_void_instr, depth_check_br);

    IRB->SetInsertPoint(depth_check_BB);

    IRB->CreateRetVoid();

    IRB->SetInsertPoint(do_carving_BB);

    BasicBlock * cur_block = do_carving_BB;

    Value * add_one_depth = IRB->CreateAdd(depth_check_val, ConstantInt::get(Int8Ty, 1));
    IRB->CreateStore(add_one_depth, depth_check_const);

    Instruction * depth_store_instr2 = IRB->CreateStore(depth_check_val, depth_check_const);

    IRB->CreateRetVoid();

    IRB->SetInsertPoint(depth_store_instr2);

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
        IRB->CreateCall(carv_name_pop, {});
      } else if (gep_pointee_type->isArrayTy()) {
        cur_block = insert_carve_probe(gep, cur_block);
        IRB->CreateCall(carv_name_pop, {});
      } else if (is_func_ptr_type(gep_pointee_type)) {
        Value * load_ptr = IRB->CreateLoad(gep_pointee_type, gep);
        Value * cast_ptr = IRB->CreateCast(Instruction::CastOps::BitCast, load_ptr, Int8PtrTy);
        std::vector<Value *> probe_args {cast_ptr};
        IRB->CreateCall(carv_func_ptr, probe_args);
        IRB->CreateCall(carv_name_pop, {});
      } else {
        Value * loadval = IRB->CreateLoad(gep_pointee_type, gep);
        cur_block = insert_carve_probe(loadval, cur_block);
        IRB->CreateCall(carv_name_pop, {});
      }
      elem_idx ++;
    }
  }

  IRB->restoreIP(cur_ip);
  std::vector<Value *> carver_args {struct_ptr};
  IRB->CreateCall(struct_carver, carver_args);
  return;
}