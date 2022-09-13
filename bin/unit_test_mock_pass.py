#!/usr/bin/python3

import sys, os
import subprocess as sp
from pathlib import Path
import tools.utils as utils

if len(sys.argv) <= 2:
  print("usage : {} <input.bc> [<compile args> ...]".format(sys.argv[0]))
  exit()

inputbc = sys.argv[1]
compile_args = sys.argv[2:]


if utils.check_given_bitcode(inputbc) == False:
  exit()

ld_path = utils.get_ld_path()

#get unit_test_pass.so filepath
source_path = Path(__file__).resolve()
source_dir = str(source_path.parent.parent)
so_path = source_dir + "/lib/unit_test_pass.so"

env=os.environ.copy()

outname = ".".join(inputbc.split(".")[:-1]) + ".unit.driver"

# cmd = ["opt", "-enable-new-pm=0", "-load", so_path, "--driver", "-o", "out.bc", inputbc]

# print(" ".join(cmd))

# process= sp.Popen(cmd, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf-8")

# while True:
#   line = process.stdout.readline()
#   if line == '' and process.poll() != None:
#     break

#   print(line.strip(), flush=True)

# cmd = ["clang++", "--coverage", "-g", "-O0", "out.bc", "-o", outname
#   #, "-fsanitize=address"
#   , "-g", "-O0"
#   , "-L", source_dir + "/lib", "-l:driver.a", "-l:unit_test_mock.a" ] + compile_args

# #env["DUMP_IR"] = "1"
# print(" ".join(cmd))

# process= sp.Popen(cmd, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf-8")

# while True:
#   line = process.stdout.readline()
#   if line == '' and process.poll() != None:
#     break

#   print(line.strip(), flush=True)

cmd = ["clang++", "--ld-path=" + ld_path, "-fno-experimental-new-pass-manager"
  #, "-D_GLIBCXX_DEBUG"
  , "-Xclang", "-load", "-Xclang", so_path, "-fPIC", "-ggdb", "-O0"
  , "-I", source_dir + "/include", "-o", outname
  , "-L", source_dir + "/lib", inputbc, "-l:driver.a", "-l:unit_test_mock.a" ] + compile_args

#env["DUMP_IR"] = "1"
print(" ".join(cmd))

process= sp.Popen(cmd, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf-8")

while True:
  line = process.stdout.readline()
  if line == '' and process.poll() != None:
    break

  print(line.strip(), flush=True)