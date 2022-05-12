#!/usr/bin/python3

import os
import subprocess as sp

if not os.path.isfile("src/carver.o"):
  print("Can't find carver.o file!")
  exit()

probe_lists = []

with open("src/probes.txt", "r") as f:
  for line in f:
    probe_lists.append(line.strip())

cmd = ["nm", "src/carver.o"]
res = sp.run(cmd, stdout=sp.PIPE).stdout

linking_names = dict()
for line in res.decode().split("\n"):
  for p in probe_lists:
    if p in line:
      linking_names[p] = line.split(" ")[-1]

with open("lib/probe_names.txt", "w") as f:
  for probe in linking_names:
    f.write(probe + " " + linking_names[probe] + "\n")