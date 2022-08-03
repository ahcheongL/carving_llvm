#!/usr/bin/python3

import sys, os
import subprocess as sp
from utils import check_given_bitcode, get_ld_path
from pathlib import Path

def run_read_gtest_command(inputbc, compile_args):

  #check given file exists
  if not check_given_bitcode(inputbc):
    return

  #get ld.lld path
  cmd = ["llvm-config", "--bindir"]
  out = sp.run(cmd, stdout=sp.PIPE, stderr=sp.PIPE)

  if b"command not found" in out.stderr:
    print("Can't find llvm-config, please check PATH")
    exit()

  ld_path = get_ld_path()

  #get extract_info_pass.so filepath
  source_path = Path(__file__).resolve()
  source_dir = str(source_path.parent.parent)
  so_path = source_dir + "/lib/read_gtest.so"

  cmd = ["clang++", "--ld-path=" + ld_path, "-fno-experimental-new-pass-manager"
    , "-Xclang", "-load", "-Xclang", so_path, "-fPIC"
    , "-I", source_dir + "/include", "-o", "/dev/null"
    , "-L", source_dir + "/lib", inputbc, ] + compile_args

  env=os.environ.copy()
  #env["DUMP_IR"] = "1"

  print(" ".join(cmd))

  sp.run(cmd, env=env)

if __name__ == "__main__":
  if len(sys.argv) <= 1:
    print("usage : {} <input.bc> [<compile args> ...]".format(sys.argv[0]))
    exit()

  inputbc = sys.argv[1]
  compile_args = sys.argv[2:]
  run_read_gtest_command(inputbc, compile_args)
  exit()