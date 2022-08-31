#!/usr/bin/python3

import sys
import glob

if len(sys.argv) != 2:
  print("usage : {} <tc_dir>".format(sys.argv[0]))
  exit()

funcs = set()

f1 = open("run_tests2.sh", "w")

idx = 0
for fn in glob.glob("{}/*".format(sys.argv[1])):
  funcname = fn.split("/")[-1]
  funcs.add(funcname)
  for fn2 in glob.glob("{}/*".format(fn)):
    f1.write("./curl.{}.driver {} &\n".format(funcname, fn2))

    idx += 1

    if idx == 10:
      f1.write("wait\n")
      idx = 0

f1.write("wait\n")
f1.close()

print("# of funcs : {}".format(len(funcs)))