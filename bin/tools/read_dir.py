#!/usr/bin/python3

import glob
import subprocess as sp
import sys
from extract import run_extract_type_command
from read_gtest import run_read_gtest_command
from get_call_seq import run_get_call_seq_command


def main():
  if len(sys.argv) != 2:
    print("usage : {} <extract_type|get_call_seq|read_gtest>".format(sys.argv[0]))
    exit()

  mode = sys.argv[1]
  if mode == "extract_type":
    command = run_extract_type_command
  elif mode == "read_gtest":
    command = run_read_gtest_command
  elif mode == "get_call_seq":
    command = run_get_call_seq_command
  else:
    print("usage : {} <extract_type|read_gtest>")
    exit()

  for fn in glob.glob("./**/*") + glob.glob("./*"):
    cmd = ["file", fn]
    stdout = sp.run(cmd, stdout=sp.PIPE, stderr=sp.DEVNULL).stdout
    if b"ELF 64-bit LSB executable" not in stdout:
      continue

    cmd = ["get-bc", fn]
    sp.run(cmd, stdout=sp.DEVNULL, stderr=sp.DEVNULL)

    cmd = ["file", fn + ".bc"]
    stdout = sp.run(cmd, stdout=sp.PIPE, stderr=sp.DEVNULL).stdout
    if b"bitcode" not in stdout:
      continue
    
    command(fn + ".bc", [])

if __name__ == "__main__":
  main()