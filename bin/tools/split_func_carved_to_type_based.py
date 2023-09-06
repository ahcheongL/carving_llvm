#!/usr/bin/python3

import glob, sys, os
from utils import read_carved_func_type

import datetime

if len(sys.argv) != 3:
    print("Usage: {} <carved_dir> <carved_types.txt>".format(sys.argv[0]))
    sys.exit(1)

carved_dir = sys.argv[1]
type_infof = sys.argv[2]

type_info, types = read_carved_func_type(type_infof)

try:
  os.mkdir(carved_dir + "_type_based")
except:
  pass

file_index = dict()

for type_name in types:
  file_index[type_name] = dict()
  try:
    os.mkdir(carved_dir + "_type_based/" + type_name)
  except:
    pass

class carved_ptr:
  def __init__(self, old_index, ptr_line):
    self.old_index = old_index
    self.ptr_line = ptr_line

import shutil

for fn in glob.glob("{}/*".format(carved_dir)):
  if fn == "{}/call_seq".format(carved_dir):
    continue

  filename = fn.split("/")[-1]
  if filename[:12] == "carved_file_":
    shutil.copytree(fn, carved_dir + "_type_based/" + filename)
    continue
  
  print("Processing {}".format(fn))
  print(datetime.datetime.now())
  
  func_name = "_".join(fn.split("/")[-1].split("_")[:-2])

  if func_name not in type_info:
    continue

  var_types = type_info[func_name]
  var_idx = 0
  ptrs = []
  variables = []
  value_carved_ptrs = []

  with open(fn, "r") as f1:      

    for line in f1:
      if "####" in line:
        break
      else:
        ptr_index = int(line.split(":")[0])
        ptrs.append(carved_ptr(ptr_index, line))
    
    cur_var_lines = ""
    line_idx = 0
    var_name = var_types[var_idx][0]
    for line in f1:
      line_sp = line.strip().split(":")
      if "_ret" in line_sp[0]:
        break
      
      if var_name not in line_sp[0]:
        variables.append(cur_var_lines)
        cur_var_lines = line
        var_idx += 1
        if var_idx >= len(var_types):
          break
        var_name = var_types[var_idx][0]

      else:
        cur_var_lines += line
      
      if ":PTR:" not in line:
        continue
      
      ptr_offset = int(line.split(":")[3])
      if ptr_offset != 0:
        continue

      ptr_index = int(line.split(":")[2])

      already_carved = False
      for (ptr_idx, val_name) in value_carved_ptrs:
        if ptr_idx == ptr_index:
          already_carved = True
          break
      
      if already_carved:
        continue
      
      value_carved_ptrs.append((ptr_index, line.split(":")[0]))

    if cur_var_lines != "":
      variables.append(cur_var_lines)

    if len(variables) != len(var_types):
      print("{} has {} variables, but {} types".format(func_name, len(variables), len(var_types)))
      continue

    var_idx = 0
    for varinfo in variables:
      type_name = var_types[var_idx][1]
      varinfo = varinfo.split("\n")

      result_ptrs = []
      result = []

      for ptr in ptrs:
        result_ptrs.append(ptr.ptr_line)

      wrote_ptrs = set()
      
      for line in varinfo:
        result.append(line + "\n")
        if ":PTR:" in line:
          ptr_index = int(line.split(":")[2])
          var_name = line.split(":")[0]

          if ptr_index in wrote_ptrs:
            continue

          no_need_to_write = False
          matching_val_name = ""

          for (ptr_idx, val_name) in value_carved_ptrs:
            if ptr_idx == ptr_index and var_name == val_name:
              no_need_to_write = True
              matching_val_name = val_name
              break              

          if no_need_to_write:
            wrote_ptrs.add(ptr_index)
            continue

          with open(fn, "r") as f3:
            for line2 in f3:
              if matching_val_name in line2:
                break
            for line2 in f3:
              if matching_val_name not in line2:
                break
              result.append(line2)
                      
          wrote_ptrs.add(ptr_index)

      if func_name not in file_index[type_name]:
        file_index[type_name][func_name] = 0
      
      cur_f_index = file_index[type_name][func_name]
      file_index[type_name][func_name] += 1

      carved_filename = carved_dir + "_type_based/" + type_name + "/" + func_name \
        + "_" + str(cur_f_index) + ".txt"

      with open(carved_dir + "_type_based/" + type_name + "/" + func_name
        + "_" + str(cur_f_index) + ".txt", "w") as f2:

        ptr_map = dict()
        for ptr_line in result_ptrs:
          old_index = int(ptr_line.split(":")[0])
          if old_index not in wrote_ptrs:
            continue
          new_ptr_index = len(ptr_map)
          ptr_map[old_index] = new_ptr_index
          f2.write("{}:{}".format(new_ptr_index, ":".join(ptr_line.split(":")[1:])))
        
        f2.write("####\n")

        for line in result:
          if len(line) <= 1:
            continue
          elif ":PTR:" in line:
            old_index = int(line.split(":")[2])
            if old_index not in ptr_map:
              f2.write(line)
              continue
            line = line.split(":")
            line[2] = str(ptr_map[old_index])
            line = ":".join(line)
          f2.write(line)

      var_idx +=1

for type_name in types:
  type_dirn = carved_dir + "_type_based/" + type_name
  carved_obj_fns = glob.glob("{}/*".format(type_dirn))
  if len(carved_obj_fns) == 0:
    os.rmdir(type_dirn)