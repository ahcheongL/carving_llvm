
import sys

if len(sys.argv) != 3:
  print("usage : {} <carved_dir> <outdir> ".format(sys.argv[0]))
  sys.exit(1)

carved_dir = sys.argv[1]
outdir = sys.argv[2]

import glob, os, shutil

try:
  os.mkdir(outdir)
except:
  pass

hashmap = dict()

for sys_dirn in glob.glob("{}/*_type_based".format(carved_dir)):
  for type_dirn in glob.glob("{}/*".format(sys_dirn)):
    carved_obj_fns = glob.glob("{}/*".format(type_dirn))
    if len(carved_obj_fns) == 0:
      continue
    
    typen = type_dirn.split("/")[-1]

    outfn = "{}/{}".format(outdir, typen)
    try:
      os.mkdir(outfn)
    except:
      pass

    if typen not in hashmap:
      hashmap[typen] = set()
    
    for carved_obj_fn in carved_obj_fns:
      hash_val = 0
      with open(carved_obj_fn, "rb") as f1:
        for line in f1:
          hash_val = hash_val + hash(line)

      if hash_val not in hashmap[typen]:
        shutil.copyfile(carved_obj_fn, "{}/{}".format(outfn, carved_obj_fn.split("/")[-1]))
        hashmap[typen].add(hash_val)


