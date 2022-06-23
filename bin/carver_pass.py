#!/usr/bin/python3

import sys, os
import subprocess as sp
from pathlib import Path

if len(sys.argv) <= 1:
  print("usage : {} <input.bc> [<compile args> ...]".format(sys.argv[0]))
  exit()

inputbc = sys.argv[1]
compile_args = sys.argv[2:]

#check given file exists
if not os.path.isfile(inputbc):
  print("Can't find file : {}".format(inputbc))
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
so_path = source_dir + "/lib/carver_pass.so"

outname = ".".join(inputbc.split(".")[:-1]) + ".rel.carv"

cmd = ["clang++", "--ld-path=" + ld_path, "-fno-experimental-new-pass-manager"
  #, "-D_GLIBCXX_DEBUG"
  , "-Xclang", "-load", "-Xclang", so_path, "-fPIC"#, "-ggdb", "-O0"
  , "-I", source_dir + "/include", "-o", outname#, #"-fsanitize=address"
  , "-L", source_dir + "/lib", inputbc, "-l:carver.a" ] + compile_args

env=os.environ.copy()
#env["DUMP_IR"] = "1"

print(" ".join(cmd))

sp.run(cmd, env=env)
