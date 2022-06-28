#!/usr/bin/python3

import os,sys
import subprocess as sp

if len(sys.argv) != 4:
  print("Usage : python3 {} <input.o> <input.txt> <output.txt>".format(sys.argv[0]))
  exit()

inputf = sys.argv[1]
inputtxt = sys.argv[2]
outputtxt = sys.argv[3]

if not os.path.isfile(inputf):
  print("Can't find ", inputf, " file!")
  exit()

probe_lists = []

with open(inputtxt, "r") as f:
  for line in f:
    probe_lists.append(line.strip())

cmd = ["nm", inputf]
res = sp.run(cmd, stdout=sp.PIPE).stdout

linking_names = dict()
for line in res.decode().split("\n"):
  for p in probe_lists:
    if p in line:
      linking_names[p] = line.split(" ")[-1]

with open(outputtxt, "w") as f:
  for probe in linking_names:
    f.write(probe + " " + linking_names[probe] + "\n")