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

  std::set<std::string> complex_types;

  std::set<Function *> checking_funcs;

  std::pair<bool, std::string> is_worth_to_test(Function * F);
  unsigned int get_num_fields(StructType * type);

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

  return false;
}

std::pair<bool, std::string> extract_pass::is_worth_to_test(Function * F) {
  bool to_report = false;
  std::string type_name;

  if (checking_funcs.find(F) != checking_funcs.end()) {
    return std::make_pair(to_report, type_name);
  }

  checking_funcs.insert(F);

  for (auto &BB : F->getBasicBlockList()) {
    for (auto &I : BB) {
      if (isa<CallBase>(&I)) {
        CallBase * call_inst = dyn_cast<CallBase>(&I);
        Function * called_func = call_inst->getCalledFunction();

        unsigned int num_op = call_inst->getNumOperands();
        unsigned int idx;

        for (idx = 0; idx < num_op; idx++) {
          Value * operand = call_inst->getOperand(idx);
          Type * op_type = operand->getType();
          if (op_type->isStructTy()) {
            StructType * struct_type = dyn_cast<StructType>(op_type);
            std::string struct_name = struct_type->getName().str();
            for (auto complex_type : complex_types) {
              if (struct_name.find(complex_type) != std::string::npos) {
                to_report = true;
                type_name = struct_name;
                break;
              }
            }
          } else if (op_type->isPointerTy()) {
            PointerType * pointer_type = dyn_cast<PointerType>(op_type);
            if (pointer_type->getElementType()->isStructTy()) {
              StructType * struct_type = dyn_cast<StructType>(pointer_type->getElementType());
              std::string struct_name = struct_type->getName().str();
              for (auto complex_type : complex_types) {
                if (struct_name.find(complex_type) != std::string::npos) {
                  to_report = true;
                  type_name = struct_name;
                  break;
                }
              }
            }
          }
        }

        if (called_func == nullptr) { continue; }
        
        if (!to_report) {
          auto res2 = is_worth_to_test(called_func);
          if (res2.first) {
            to_report = true;
            type_name = res2.second;
          }
        }
      }

      if (to_report) { break; }
    }
    if (to_report) { break; }
  }

  checking_funcs.erase(F);

  return std::make_pair(to_report, type_name);
}

bool extract_pass::hookInstrs(Module &M) {
  LLVMContext &              C = M.getContext();
  const DataLayout & dataLayout = M.getDataLayout();
  Mod = &M;
  Context = &C;
  DL = &dataLayout;
  IRB = new IRBuilder<> (C);
  
  DbgFinder.processModule(M);

  std::ifstream typefile("types.txt");

  if (typefile.fail()) {
    DEBUG0("Failed to open funcs.txt\n");
    return false;
  }

  std::string line;
  while(std::getline(typefile, line)) {
    complex_types.insert(line);
  }

  typefile.close();

  std::string outfile_name = M.getName().str() + ".info";
  std::ofstream outfile;
  outfile.open(outfile_name);

  if (complex_types.size() == 0) {
    delete IRB;

    outfile.close();
    return true;
  }

  for (auto &F : M) {
    if (F.size() == 0) { continue; }

    std::string func_name = F.getName().str();
    if (func_name.find("TestBody") == std::string::npos) { continue; }

    auto res = is_worth_to_test(&F);

    if (res.first) {
      auto DIscope = F.getSubprogram()->getScope();

      if (DIscope && isa<DICompositeType>(DIscope)) {
        DICompositeType * DIComposite = dyn_cast<DICompositeType>(DIscope);
        func_name = DIComposite->getName().str();
      }

      outfile << func_name << " uses " << res.second << "\n";
    } else {
      outfile << "#" << func_name << "\n";
    }

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

