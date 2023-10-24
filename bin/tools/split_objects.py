#!/usr/bin/python3

import glob, sys, os, shutil

import datetime

if len(sys.argv) != 2:
    print("Usage: {} <carved_dir>".format(sys.argv[0]))
    sys.exit(1)

carved_dir = sys.argv[1]

if carved_dir[-1] != "/":
    carved_dir += "/"


object_dir = carved_dir + "objects/"

shutil.rmtree(object_dir, ignore_errors=True)
os.mkdir(object_dir)

file_index = dict()

for fn in glob.glob("{}/*".format(carved_dir)):
    if "call_seq" in fn:
        continue

    if "/objects" in fn:
        continue

    filename = fn.split("/")[-1]
    if filename[:12] == "carved_file_":
        try:
            os.mkdir(object_dir + filename)
        except:
            pass
        existing_files = glob.glob(object_dir + filename + "/*")
        num_existing_files = len(existing_files)
        for carved_file_fn in glob.glob(fn + "/*"):
            shutil.copy(
                carved_file_fn,
                object_dir + filename + "/" + str(num_existing_files),
            )
            num_existing_files += 1
        continue

    print("Processing {}".format(fn))
    print(datetime.datetime.now())

    func_name = "_".join(fn.split("/")[-1].split("_")[:-2])

    var_idx = 0
    all_ptrs = []  # list of <ptr line>
    cur_obj_type = ""
    cur_obj = []
    cur_obj_used_ptrs = []  # list of <ptr line>
    cur_used_ptrs_idx_map = dict()  # old_index -> new_index

    with open(fn, "r") as f1:
        print("Reading " + fn)
        for line in f1:
            if "####" in line:
                break
            else:
                all_ptrs.append(line.strip())

        for line in f1:
            line = line.strip()
            if "OBJ_INFO" in line:
                if len(cur_obj) != 0:
                    num_existing_files = len(
                        glob.glob(object_dir + glob.escape(cur_obj_type) + "*")
                    )

                    new_fn = object_dir + cur_obj_type + "_" + str(num_existing_files)
                    print("Writing to {}".format(new_fn))

                    with open(new_fn, "w") as f2:
                        for ptr_line in cur_obj_used_ptrs:
                            f2.write(ptr_line + "\n")
                        f2.write("####\n")
                        for obj_line in cur_obj:
                            f2.write(obj_line + "\n")

                cur_obj.clear()
                cur_obj_used_ptrs.clear()
                cur_used_ptrs_idx_map.clear()
                cur_obj_type = ":".join(line.split(":")[2:])
                print("Processing type {}".format(cur_obj_type))

            elif line[:4] == "PTR:":
                old_index = int(line.split(":")[1])
                ptr_offset = int(line.split(":")[2])
                if old_index in cur_used_ptrs_idx_map:
                    new_index = cur_used_ptrs_idx_map[old_index]
                    cur_obj.append("PTR:{}:{}".format(new_index, ptr_offset))
                else:
                    new_index = len(cur_obj_used_ptrs)
                    cur_obj.append("PTR:{}:{}".format(new_index, ptr_offset))
                    ptr_line = all_ptrs[old_index]
                    ptr_line = ptr_line.split(":")
                    ptr_line = "{}:{}".format(new_index, ":".join(ptr_line[1:]))
                    cur_obj_used_ptrs.append(ptr_line)
                    cur_used_ptrs_idx_map[old_index] = new_index
            else:
                cur_obj.append(line)

        if len(cur_obj) != 0:
            num_existing_files = len(
                glob.glob(object_dir + glob.escape(cur_obj_type) + "*")
            )

            new_fn = object_dir + cur_obj_type + "_" + str(num_existing_files)
            print("Writing to {}".format(new_fn))

            with open(new_fn, "w") as f2:
                for ptr_line in cur_obj_used_ptrs:
                    f2.write(ptr_line + "\n")
                f2.write("####\n")
                for obj_line in cur_obj:
                    f2.write(obj_line + "\n")
