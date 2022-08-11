#!/usr/bin/python3

import glob

types = set()

for fn in glob.glob("*.info") + glob.glob("**/*.info"):
  with open(fn, "r") as f1:
    first_line = f1.readline().strip().split(" : ")
    if len(first_line) != 2:
      continue

    num_type = 1

    for line in f1:
      typename = line.strip().split(" : ")[0]
      if "." in typename:
        try:
          int(typename.split(".")[-1])
          typename = ".".join(typename.split(".")[:-1])
        except:
          pass
      types.add(typename)            
      num_type += 1

for ty in types:
  print(ty)