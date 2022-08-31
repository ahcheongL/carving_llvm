#!/usr/bin/python3

import sys
import glob

from utils import read_carved_func_type


if len(sys.argv) != 4:
  print("usage : {} <carved_dir> <carved_types.txt> <target_funcs.txt>".format(sys.argv[0]))
  exit()

type_info, types = read_carved_func_type(sys.argv[2])

funcs = set()
for fn in glob.glob("{}/*".format(sys.argv[1])):
  if "call_seq" in fn:
    continue
  
  fn = "_".join(fn.split("/")[-1].split("_")[:-2])

  funcs.add(fn)

print("# of funcs : {}".format(len(funcs)))

with open (sys.argv[3], "w") as f:
  for fn in funcs:
    f.write("{}\n".format(fn))

for funcname in type_info:
  funcs.add(funcname)


#llvm-cov
#-L /usr/lib/llvm-13/lib/clang/13.0.1/lib/linux/ -l:libclang_rt.profile-x86_64.a

with open ("make_driver.sh", "w") as f:
  f.write("#!/bin/bash\n")
  idx = 0
  for fn in funcs:
    f.write("/home/cheong/carving_llvm/bin/simple_unit_driver_pass.py curl.bc {} -lssl -lcrypto -lz -lpthread -ldl &\n".format(fn))
    idx += 1

    if idx == 10:
      f.write("wait\n")
      idx = 0
  f.write("wait\n")

with open ("run_tests.sh", "w") as f:
  f.write("#!/bin/bash\n")
  idx = 0
  for fn in glob.glob("{}/*".format(sys.argv[1])):
    if "call_seq" in fn:
      continue
    func = "_".join(fn.split("/")[-1].split("_")[:-2])
    f.write("./curl.{}.driver {} &\n".format(func, fn))

    idx += 1

    if idx == 10:
      f.write("wait\n")
      idx = 0
  
  f.write("wait\n")