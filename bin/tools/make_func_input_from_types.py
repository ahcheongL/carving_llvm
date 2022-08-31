#!/usr/bin/python3

import glob, sys, os
import datetime
import random
from utils import read_carved_func_type

if len(sys.argv) != 3:
  print("Usage: {} <carved_type_dir> <carved_types.txt>".format(sys.argv[0]))
  sys.exit(1)

carved_type_dir = sys.argv[1]
func_type_infof = sys.argv[2]

type_info, types = read_carved_func_type(func_type_infof)

outdir = carved_type_dir + "func_inputs"

try:
  os.mkdir(outdir)
except:
  pass

NUM_INPUT = 1000

for func in type_info:
  try:
    os.mkdir(outdir + "/" + func)
  except:
    pass

  vartypes = type_info[func]

  num_comb = 1
  for (var_name, type_name) in vartypes:
    type_name_escaped = glob.escape(type_name)
    objects = glob.glob("{}/{}/*".format(carved_type_dir, type_name_escaped))

    num_comb *= len(objects)

  if num_comb == 0:
    continue

  for input_idx in range(NUM_INPUT):
    ptr_index = 0
    ptr_string = ""
    var_string = ""
    for (var_name, type_name) in vartypes:
      type_name_escaped = glob.escape(type_name)
      objects = glob.glob("{}/{}/*".format(carved_type_dir, type_name_escaped))
      rand_idx = random.randrange(0, len(objects))
      fn = objects[rand_idx]
      f1 = open(fn, "r")
      ptr_index_begin = ptr_index
      for line in f1:
        if "####" in line:
          break
        else:
          line = line.strip().split(":")
          ptr_string += "{}:{}\n".format(ptr_index, ":".join(line[1:]))
          ptr_index += 1
      
      for line in f1:
        if len(line) <= 1:
          continue
        elif ":PTR:" in line:
          line = line.strip().split(":")
          orig_ptr_index = int(line[2])
          var_string += "{}:{}:{}\n".format(":".join(line[:2])
            , ptr_index_begin + orig_ptr_index, line[3])
        else:
          var_string += line
      
      f1.close()
    
    outfilename = "{}/{}/input_{}".format(outdir, func, input_idx)
    with open(outfilename, "w") as f2:
      f2.write(ptr_string)
      f2.write("####\n")
      f2.write(var_string)


    
      