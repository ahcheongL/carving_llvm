#!/usr/bin/python3

import sys, os
import subprocess as sp
from pathlib import Path
import tools.utils as utils

if len(sys.argv) <= 2:
  print("usage : {} <input.bc> <Func name> [<compile args> ...]".format(sys.argv[0]))
  exit()

inputbc = sys.argv[1]
func_name = sys.argv[2]
compile_args = sys.argv[3:]


if utils.check_given_bitcode(inputbc) == False:
  exit()

ld_path = utils.get_ld_path()

#get simple_unit_driver_pass.so filepath
source_path = Path(__file__).resolve()
source_dir = str(source_path.parent.parent)
so_path = source_dir + "/lib/simple_unit_driver_pass.so"

env=os.environ.copy()
env["TARGET_NAME"] = func_name

outname = ".".join(inputbc.split(".")[:-1]) + "." + func_name + ".driver"

cmd = ["clang++", "--ld-path=" + ld_path, "-fno-experimental-new-pass-manager"
  , "-Xclang", "-load", "-Xclang", so_path, "-fPIC"
  , "-I", source_dir + "/include", "-o", outname
  #, "-fsanitize=address"
  , "-ggdb", "-O0"
  , "-L", "/usr/lib/llvm-13/lib/clang/13.0.1/lib/linux", "-lclang_rt.profile-x86_64"
  , "-L", source_dir + "/lib", inputbc, "-l:simple_unit_driver.a", ] + compile_args

#env["DUMP_IR"] = "1"
print(" ".join(cmd))

process= sp.Popen(cmd, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf-8")

while True:
  line = process.stdout.readline()
  if line == '' and process.poll() != None:
    break

  print(line.strip(), flush=True)