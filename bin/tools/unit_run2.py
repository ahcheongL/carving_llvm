#!/usr/bin/python3

import sys, os, glob
import subprocess as sp
import multiprocessing


if len(sys.argv) != 4:
  print("Usage : {} <compile log> <carved_dir> <testlist>".format(sys.argv[0]))
  sys.exit(1)


compile_log = sys.argv[1]
carved_dir = sys.argv[2]
testlist = sys.argv[3]


def conf0(line):
  return "LibraryInit" in line

def conf7(line):
  return "LibraryTest" in line and "ContainedLibraryTest" not in line

def conf1(line):
  return "DefaultChannelTest" in line

def conf2(line):
  return "ContainedLibraryTest" in line

def conf3(line):
  return "AddressFamilies" in line and "AddressFamiliesAI" not in line

def conf4(line):
  return "TransportModes" in line and "TransportModesAI" not in line 

def conf5(line):
  return "AddressFamiliesAI" in line

def conf6(line):
  return "TransportModesAI" in line

def conf8(line):
  return line[:5] == "Modes"

confs = [conf0, conf1,conf2,conf3,conf4,conf5,conf6, conf7]
confs = [conf2]

for conf in confs:
  tests = []
  f1 = open(testlist, "r")
  for line in f1:
    if conf(line):
      line = line.strip()
      if "/" in line:
        suitename1 = line.split("/")[1].split(".")[0]
        testsname1 = line.split("/")[1].split(".")[1]
      else:
        suitename1 = line.split(".")[0]
        testsname1 = line.split(".")[1]
      tests.append((line.strip(), suitename1, testsname1))
  f1.close()

  configs = []

  f2 = open(compile_log, "r")
  for line in f2:
    if "Replace: " not in line:
      continue

    call_funcname = line.split(":")[1].strip()
    replaced_name = line.split(":")[-2].strip()

    testname = ""
    for (fullname1, suitename1, testname1) in tests:  
      if suitename1 in call_funcname:
        if testname1 == "_":
          testname = fullname1
          break
        testname2 = call_funcname.split(suitename1)[1].split("_Test")[0][1:]
        if testname2 == testname1:
          testname = fullname1
          break
    
    if testname == "":
      #print("Can't find testname for {}".format(call_funcname))
      continue

    configs.append((testname, replaced_name))

  f2.close()


  f2 = open("run_orig.sh", "w")

  manager = multiprocessing.Manager()

  def foo(testname, fn, ns, nf, nc, nt):
    cmd = ["./arestest.unit.driver", "--gtest_filter={}".format(testname),  fn]
    try:
      p = sp.Popen(cmd, stdout=sp.PIPE, stderr=sp.DEVNULL, encoding="utf-8")
      out, err = p.communicate(timeout=1)
    except Exception as e:
      nt.value += 1
      return

    print(" ".join(cmd), end = " : ")

    if p.returncode == -11:
      print("CRASHED")
      nc.value += 1
    elif "Fail" in out or "FAIL" in out:
      print("FAILED")
      nf.value += 1
    else:
      print("SUCCESS")
      ns.value += 1

  num_success = manager.Value("ns", 0)
  num_failed = manager.Value("nf", 0)
  num_crashed = manager.Value("nc", 0)
  num_timeout = manager.Value("nt", 0)

  config_idx = 0
  idx = 0
  num_tests = 0
  for config in configs:
    testname = config[0]
    replaced_name = config[1]
    f2.write("./arestest --gtest_filter={} &\n".format(testname))  

    idx += 1
    if idx > 10:
      f2.write("wait\n")
      idx = 0  

    fns = glob.glob("{}/func_inputs/{}/*".format(carved_dir, replaced_name))
    print("Running {} tests for {}".format(len(fns), testname))
    idx2 = 0
    running = set()
    for fn in fns:  
      p = multiprocessing.Process(target=foo, args=(testname, fn, num_success, num_failed, num_crashed, num_timeout))
      p.start()
      running.add(p)

      idx2 += 1

      if idx2 > 10:
        for p2 in running:
          p2.join()
        running.clear()
        idx2 = 0
      num_tests += 1

    for p in running:
      p.join()

    running.clear()

  f2.write("wait\n")
  f2.close()

  print("Total su:{}/fa:{}/cr:{}/tm:{}  total: {} tests".format(num_success, num_failed, num_crashed,num_timeout, num_tests))

