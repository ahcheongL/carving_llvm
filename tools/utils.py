#!/usr/bin/python3

import sys, os
import subprocess as sp

def check_given_bitcode(inputbc):
  #check given file exists
  if not os.path.isfile(inputbc):
    print("Can't find file : {}".format(inputbc))
    return False

  #check given file format
  cmd = ["file", inputbc]
  stdout = sp.run(cmd, stdout=sp.PIPE, stderr=sp.DEVNULL).stdout
  if b"bitcode" not in stdout:
    print("Can't recognize file : {}".format(inputbc))
    return False
  
  return True


def get_ld_path():
  cmd = ["llvm-config", "--bindir"]
  out = sp.run(cmd, stdout=sp.PIPE, stderr=sp.PIPE)

  if b"command not found" in out.stderr:
    print("Can't find llvm-config, please check PATH")
    exit()

  ld_path = out.stdout.decode()[:-1] + "/ld.lld"
  return ld_path