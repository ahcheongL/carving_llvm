#!/usr/bin/python3

import glob


num_readfile = 0
num_zero = 0

num_selected_gtest = 0
total_num_gtest = 0

for fn in glob.glob("**/*.info") + glob.glob("./*.info"):
  num_readfile += 1
  with open(fn, "r") as f1:
    firstline = f1.readline()
    if len(firstline) == 0:
      num_zero += 1
      continue
    
    if firstline[0] != '#':
      num_selected_gtest += 1
      print(firstline.strip())
    total_num_gtest += 1
    
    for line in f1:
      if line[0] != '#':
        num_selected_gtest += 1
        print(line.strip())
      total_num_gtest += 1

print("# of selected gtests : {}/{}".format(num_selected_gtest, total_num_gtest))
print("# of zero file = {}/{}".format(num_zero, num_readfile))