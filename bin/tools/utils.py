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

def read_carved_func_type(type_infof):
  type_info = dict() # func_name -> [(var_name, type_name)]
  types = set()
  with open(type_infof, "r") as f1:
    funcname = ""
    for line in f1:
      if line.startswith("##"):
        funcname = line.strip()[2:]
      elif line.startswith("**"):
        line = line.strip().split(" : ")
        var_name = line[0][2:]
        type_name = line[1]
        if funcname not in type_info:
          type_info[funcname] = []
        type_info[funcname].append((var_name, type_name))
        types.add(type_name)
      else:
        print("Wrong format!\n")
        break
  
  return type_info, types
