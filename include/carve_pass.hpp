#ifndef CARVE_PASS_UTILS_HPP
#define CARVE_PASS_UTILS_HPP

#include "pass.hpp"

void get_carving_func_callees_and_globals();

extern FunctionCallee mem_allocated_probe;
extern FunctionCallee remove_probe;

extern FunctionCallee record_func_ptr;
extern FunctionCallee add_no_stub_func;
extern FunctionCallee is_no_stub;

extern FunctionCallee argv_modifier;
extern FunctionCallee __carv_fini;

extern FunctionCallee carv_char_func;
extern FunctionCallee carv_short_func;
extern FunctionCallee carv_int_func;
extern FunctionCallee carv_long_func;
extern FunctionCallee carv_longlong_func;
extern FunctionCallee carv_float_func;
extern FunctionCallee carv_double_func;
extern FunctionCallee carv_ptr_func;
extern FunctionCallee carv_func_ptr;

extern FunctionCallee carv_ptr_name_update;
extern FunctionCallee struct_name_func;
extern FunctionCallee carv_name_push;
extern FunctionCallee carv_name_pop;

extern FunctionCallee carv_func_call;
extern FunctionCallee carv_func_ret;

extern FunctionCallee update_carved_ptr_idx;
extern FunctionCallee keep_class_name;
extern FunctionCallee class_carver;

extern FunctionCallee carv_open;
extern FunctionCallee carv_close;

//Memory tracking
extern std::vector<AllocaInst *> tracking_allocas;
void Insert_alloca_probe(BasicBlock &);
void insert_dealloc_probes();
bool Insert_mem_func_call_probe(Instruction *, std::string);


void Insert_carving_main_probe(BasicBlock *, Function *);

BasicBlock * insert_carve_probe(Value *, BasicBlock *);

extern std::set<std::string> struct_carvers;
void insert_struct_carve_probe(Value *, Type *);
void insert_struct_carve_probe_inner(Value *, Type *);

BasicBlock * insert_gep_carve_probe(Value * gep_val, BasicBlock * cur_block);
BasicBlock * insert_array_carve_probe(Value * arr_ptr_val, BasicBlock * cur_block);

void insert_check_carve_ready();

extern Constant * global_carve_ready;
extern Constant * global_cur_class_idx;
extern Constant * global_cur_class_size;

void gen_class_carver();

#endif