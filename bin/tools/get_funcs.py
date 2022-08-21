#!/usr/bin/python3

import sys
import glob

if len(sys.argv) != 2:
  print("usage : {} <carved_dir>".format(sys.argv[0]))
  exit()

funcs = set()
for fn in glob.glob("{}/*".format(sys.argv[1])):
  if "call_seq" in fn:
    continue
  
  fn = "_".join(fn.split("/")[-1].split("_")[:-2])

  funcs.add(fn)

print("# of funcs : {}".format(len(funcs)))

with open ("carved_funcs.txt", "w") as f:
  for fn in funcs:
    f.write("{}\n".format(fn))


with open ("make_driver.sh", "w") as f:
  f.write("#!/bin/bash\n")
  idx = 0
  for fn in funcs:
    f.write("/home/cheong/carving_llvm/bin/simple_unit_driver_pass.py arestest.bc {} -lstdc++ -lm -lpthread &\n".format(fn))
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
    f.write("./arestest.{}.driver {} &\n".format(func, fn))

    idx += 1

    if idx == 10:
      f.write("wait\n")
      idx = 0
  
  f.write("wait\n")