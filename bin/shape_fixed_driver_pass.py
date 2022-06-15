#!/usr/bin/python3

import sys, os
import subprocess as sp
from pathlib import Path

if len(sys.argv) <= 2:
  print("usage : {} <input.bc> <Func idx> [<compile args> ...]".format(sys.argv[0]))
  exit()

inputbc = sys.argv[1]
func_idx = sys.argv[2]
compile_args = sys.argv[3:]


#check given file exists
if not os.path.isfile(inputbc):
  print("Can't find file : {}".format(inputbc))
  exit()

funcs_txt_file_path = "/".join(inputbc.split("/")[:-1]) + "funcs.txt"
if not os.path.isfile(funcs_txt_file_path):
  print("This tool assumes funcs.txt file located in the same directory with input bitcode file")
  exit()

#check given file format
cmd = ["file", inputbc]
stdout = sp.run(cmd, stdout=sp.PIPE, stderr=sp.DEVNULL).stdout
if b"bitcode" not in stdout:
  print("Can't recognize file : {}".format(inputbc))
  exit()

#get ld.lld path
cmd = ["llvm-config", "--bindir"]
out = sp.run(cmd, stdout=sp.PIPE, stderr=sp.PIPE)

if b"command not found" in out.stderr:
  print("Can't find llvm-config, please check PATH")
  exit()

ld_path = out.stdout.decode()[:-1] + "/ld.lld"

#get carver_pass.so filepath
source_path = Path(__file__).resolve()
source_dir = str(source_path.parent.parent)
so_path = source_dir + "/lib/shape_fixed_driver_pass.so"

funcs = []
with open(funcs_txt_file_path) as f1:
  for line in f1:
    funcs.append(line.strip())

if len(funcs) == 0:
  print("Can't get target functions")
  exit()

env=os.environ.copy()
env["FUNCIDX"] = func_idx
func_name = funcs[int(func_idx)]

outname = ".".join(inputbc.split(".")[:-1]) + "." + func_name + ".driver"

cmd = ["clang++", "--ld-path=" + ld_path, "-fno-experimental-new-pass-manager"
  #, "-D_GLIBCXX_DEBUG"
  , "-Xclang", "-load", "-Xclang", so_path, "-fPIC", "-ggdb", "-O0"
  , "-I", source_dir + "/include", "-o", outname, "-fsanitize=address"
  , "-L", source_dir + "/lib", inputbc, "-l:shape_fixed_driver.a" ] + compile_args

env["FUNCIDX"] = str(func_idx)
#env["DUMP_IR"] = "1"
print(" ".join(cmd))
try:
  sp.run(cmd, env=env)
except:
  pass
