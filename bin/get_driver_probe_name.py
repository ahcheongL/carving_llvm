#!/usr/bin/python3

import os
import subprocess as sp

if not os.path.isfile("src/driver.o"):
  print("Can't find driver.o file!")
  exit()

probe_lists = []

with open("src/driver_probes.txt", "r") as f:
  for line in f:
    probe_lists.append(line.strip())

cmd = ["nm", "src/driver.o"]
res = sp.run(cmd, stdout=sp.PIPE).stdout

linking_names = dict()
for line in res.decode().split("\n"):
  for p in probe_lists:
    if p in line:
      linking_names[p] = line.split(" ")[-1]

with open("lib/driver_probe_names.txt", "w") as f:
  for probe in linking_names:
    f.write(probe + " " + linking_names[probe] + "\n")