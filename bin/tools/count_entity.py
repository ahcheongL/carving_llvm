#!/usr/bin/python3

import sys
import glob

if len(sys.argv) != 2:
  print("usage : {} <carved_dir>".format(sys.argv[0]))
  exit()


carved_dir = sys.argv[1]

line_count = []

entities = dict()
types = ["char", "short", "int", "long", "longlong", "float", "double", "func_ptr", "Unknown_ptr", "null", "ptr"]
for ty in types:
  entities[ty] = 0

for fn in glob.glob("{}/*".format(carved_dir)):
  if "call_seq" in fn:
    continue
  
  with open(fn, "r") as f:
    is_carved_entity = False
    num_line = 0
    for line in f:
      num_line += 1
      if is_carved_entity:
        if "CHAR" in line:
          entities["char"] += 1
        elif "SHORT" in line:
          entities["short"] += 1
        elif "INT" in line:
          entities["int"] += 1
        elif "LONGLONG" in line:
          entities["longlong"] += 1
        elif "LONG" in line:
          entities["long"] += 1
        elif "NULL" in line:
          entities["null"] += 1
        elif "UNKNOWN_PTR" in line:
          entities["Unknown_ptr"] += 1
        elif "FLOAT" in line:
          entities["float"] += 1
        elif "DOUBLE" in line:
          entities["double"] += 1
        elif "FUNCPTR" in line:
          entities["func_ptr"] += 1
        elif "PTR" in line:
          entities["ptr"] += 1
      else:
        if line[0] == "#":
          is_carved_entity = True
      line_count.append((num_line, fn))


for ty in types:
  print("{} : {}".format(ty, entities[ty]))

line_count.sort(key=lambda x: x[0])

for fn in line_count:
  print("{} : {}".format(fn[0], fn[1]))
