#!/usr/bin/python3

import sys, os
import subprocess as sp
from pathlib import Path
import tools.utils as utils
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('-i', dest='input', nargs=1, required=True ,help='input bitcode')
parser.add_argument('-f', dest='func', nargs=1, help='target function')
parser.add_argument('-c', dest="compile_args", nargs=argparse.REMAINDER, help='compile args')
args = parser.parse_args()

inputbc = args.input[0]
func_name = args.func
compile_args = args.compile_args


if utils.check_given_bitcode(inputbc) == False:
  exit()

ld_path = utils.get_ld_path()

if compile_args == None:
  compile_args = utils.get_link_option(inputbc)

#get simple_unit_driver_pass.so filepath
source_path = Path(__file__).resolve()
source_dir = str(source_path.parent.parent)
so_path = source_dir + "/lib/fuzz_driver_pass.so"

env=os.environ.copy()

if func_name != None:
  func_name = func_name[0]
  env["TARGET_NAME"] = func_name
  outname = ".".join(inputbc.split(".")[:-1]) + "." + func_name + ".driver"
else:
  outname = "/dev/null"

cmd = ["clang++", "--ld-path=" + ld_path, "-fno-experimental-new-pass-manager"
  , "-Xclang", "-load", "-Xclang", so_path, "-fPIC"
  , "-I", source_dir + "/include", "-o", outname
  #, "-fsanitize=address"
  , "-ggdb", "-O0"
  , "-L", source_dir + "/lib", inputbc, "-l:driver.a", "-l:fuzz_driver.a", ] + compile_args

#env["DUMP_IR"] = "1"
print(" ".join(cmd))

process= sp.Popen(cmd, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf-8")

while True:
  line = process.stdout.readline()
  if line == '' and process.poll() != None:
    break

  print(line.strip(), flush=True)
