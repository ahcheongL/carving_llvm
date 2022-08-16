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