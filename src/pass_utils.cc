
#include "pass.hpp"
std::vector<Value *> empty_args;

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
    Constant * new_global = IRB->CreateGlobalStringPtr(name);
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

int num_class_name_const = 0;
std::vector<std::pair<Constant *, int>> class_name_consts;
std::map<StructType *, std::pair<int, Constant *>> class_name_map;

void get_class_type_info(Module * Mod, IRBuilder<>* IRB, const DataLayout * DL) {
  for (auto struct_type : Mod->getIdentifiedStructTypes()) {
    std::string name = get_type_str(struct_type);
    Constant * name_const = gen_new_string_constant(name, IRB);
    if (struct_type->isOpaque()) { continue; }
    class_name_consts.push_back(std::make_pair(name_const, DL->getTypeAllocSize(struct_type)));
    class_name_map.insert(std::make_pair(struct_type
      , std::make_pair(num_class_name_const++, name_const)));
  }
}