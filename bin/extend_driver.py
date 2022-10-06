#!/usr/bin/python3

import sys, os
import subprocess as sp
from pathlib import Path
import tools.utils as utils

if len(sys.argv) <= 1:
  print("usage : {} <input.bc> [<compile args> ...]".format(sys.argv[0]))
  exit()

inputbc = sys.argv[1]
compile_args = sys.argv[2:]

if utils.check_given_bitcode(inputbc) == False:
  exit()

ld_path = utils.get_ld_path()

if len(compile_args) == 0:
  compile_args = utils.get_link_option(inputbc)

#get extend_driver_pass.so filepath
source_path = Path(__file__).resolve()
source_dir = str(source_path.parent.parent)
so_path = source_dir + "/lib/extend_driver_pass.so"

env=os.environ.copy()

outname = ".".join(inputbc.split(".")[:-1]) + ".driver"

cmd = ["clang++", "--ld-path=" + ld_path, "-fno-experimental-new-pass-manager"
  , "-Xclang", "-load", "-Xclang", so_path, "-fPIC"
  , "-I", source_dir + "/include", "-o", outname
  #, "-fsanitize=address"
  , "-ggdb", "-O0"
  , "-L", source_dir + "/lib", inputbc, "-l:extend_driver.a", "-l:driver.a"] + compile_args

#env["DUMP_IR"] = "1"
print(" ".join(cmd))

process= sp.Popen(cmd, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf-8")

while True:
  line = process.stdout.readline()
  if line == '' and process.poll() != None:
    break

  print(line.strip(), flush=True)