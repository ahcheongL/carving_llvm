#include"carve_pass.hpp"

namespace {

class extract_pass : public ModulePass {

 public:
  static char ID;
  extract_pass() : ModulePass(ID) { func_id = 0;}

  bool runOnModule(Module &M) override;

#if LLVM_VERSION_MAJOR >= 4
  StringRef getPassName() const override {
#else
  const char *getPassName() const override {
#endif
    return "extract info instrumentation";
  }

 private:
  bool hookInstrs(Module &M);
  
  DebugInfoFinder DbgFinder;
  Module * Mod;
  LLVMContext * Context;
  const DataLayout * DL;
  IRBuilder<> *IRB;

  unsigned int get_num_fields(StructType * type);

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

  int func_id;
};

}  // namespace

char extract_pass::ID = 0;

unsigned int extract_pass::get_num_fields(StructType * type) {
  unsigned int num_fields = 0;
  for (unsigned int i = 0; i < type->getNumElements(); i++) {
    if (type->getElementType(i)->isStructTy()) {
      num_fields += get_num_fields(dyn_cast<StructType>(type->getElementType(i)));
    } else {
      num_fields++;
    }
  }
  return num_fields;
}

static bool exclude_struct(std::string name) {
  if (name.find("std::") != std::string::npos) { return true; }
  if (name.find("struct._IO_FILE") != std::string::npos) { return true; }
  if (name.find(".anon.") != std::string::npos) { return true; }
  if (name.find("__gnu_cxx::") != std::string::npos) { return true; }

  if (name.find("google::protobuf") != std::string::npos) { return true; }
  if (name.find("testing::") != std::string::npos) { return true; }

  size_t name_len = name.size();
  if ((name_len > 4) && (name.substr(name_len - 4) == "Test")) { return true; }
  if ((name_len > 9) && (name.substr(name_len - 9) == "Test.base")) { return true; }

  if (name.find("opencv_test") != std::string::npos) { return true; }

  return false;
}

bool extract_pass::hookInstrs(Module &M) {
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

#define FIELD_THRESHOLD 30

  std::vector<std::pair<unsigned int, StructType *>> fields_info;

  for (auto struct_type : Mod->getIdentifiedStructTypes()) {
    if (struct_type->isOpaque()) { continue; }
    std::string name = struct_type->getName().str();
    unsigned int num_fields = get_num_fields(struct_type);

    if (exclude_struct(name)) { continue; }

    if (num_fields > FIELD_THRESHOLD) {
      fields_info.push_back(std::make_pair(num_fields, struct_type));
    } else {
      for (auto field_type : struct_type->elements()) {
        if (field_type->isPointerTy()) {
          PointerType * ptr_type = dyn_cast<PointerType>(field_type);
          if (ptr_type->getElementType()->isStructTy()) {
            StructType * struct_type = dyn_cast<StructType>(ptr_type->getElementType());
            if (struct_type->isOpaque()) { continue; }
            std::string name = struct_type->getName().str();
            unsigned int num_fields = get_num_fields(struct_type);
            if (exclude_struct(name)) { continue; }
            if (num_fields > FIELD_THRESHOLD) {
              fields_info.push_back(std::make_pair(num_fields, struct_type));
            }
          }
        }
      }
    }

  }

  auto sort_cmp = [] (std::pair<unsigned int, StructType *> a
    , std::pair<unsigned int, StructType *> b) {
    return a.first > b.first;
  };

  std::sort(fields_info.begin(), fields_info.end(), sort_cmp);

  std::string outfile_name = M.getName().str() + ".info";
  std::ofstream outfile;
  outfile.open(outfile_name);

  for (auto struct_type : fields_info) {
    outfile << struct_type.second->getName().str() << " : " << struct_type.first << "\n";
  }
  
  outfile.close();
  delete IRB;

  for (auto &F : M) {
    F.dropAllReferences();
    F.eraseFromParent();
  }
  return true;
}

bool extract_pass::runOnModule(Module &M) {

  DEBUG0("Running extract_pass\n");

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

static void registerextract_passPass(const PassManagerBuilder &,
                                           legacy::PassManagerBase &PM) {

  auto p = new extract_pass();
  PM.add(p);

}

static RegisterStandardPasses Registerextract_passPass(
    PassManagerBuilder::EP_OptimizerLast, registerextract_passPass);

static RegisterStandardPasses Registerextract_passPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerextract_passPass);

static RegisterStandardPasses Registerextract_passPassLTO(
    PassManagerBuilder::EP_FullLinkTimeOptimizationLast,
    registerextract_passPass);

