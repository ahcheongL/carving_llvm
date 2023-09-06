#ifndef DRIVER_PASS_UTILS_HPP
#define DRIVER_PASS_UTILS_HPP

#include "pass.hpp"

extern FunctionCallee __inputf_open;

extern FunctionCallee replay_char_func;
extern FunctionCallee replay_short_func;
extern FunctionCallee replay_int_func;
extern FunctionCallee replay_long_func;
extern FunctionCallee replay_longlong_func;
extern FunctionCallee replay_float_func;
extern FunctionCallee replay_double_func;

extern FunctionCallee replay_ptr_func;

extern FunctionCallee replay_func_ptr;
extern FunctionCallee record_func_ptr;

extern FunctionCallee update_class_ptr;

extern FunctionCallee keep_class_info;

extern FunctionCallee __replay_fini;

extern FunctionCallee class_replay;

extern Constant *global_cur_class_index;
extern Constant *global_cur_class_size;
extern Constant *global_ptr_alloc_size;
extern Constant *global_cur_zero_address;

void get_driver_func_callees();

void make_stub(Function *F);

Value *insert_replay_probe(Type *, Value *);

void insert_gep_replay_probe(Value *);

void insert_struct_replay_probe_inner(Value *, Type *);
void insert_struct_replay_probe(Value *, Type *);

void gen_class_replay();

extern Constant *global_cur_class_index;
extern Constant *global_cur_class_size;
#endif