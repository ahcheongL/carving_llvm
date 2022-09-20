#!/usr/bin/python3

import sys
import glob

from utils import read_carved_func_type
from pathlib import Path
import os

if len(sys.argv) != 5:
    print(
        "usage : {} <.bc file> <carved_dir> <carved_types.txt> <target_funcs.txt>".format(
            sys.argv[0]
        )
    )
    exit()


script_file_path = Path(os.path.realpath(__file__))
project_path = script_file_path.parent.parent.parent
unit_driver_pass_py = project_path / "bin" / "simple_unit_driver_pass.py"
bc_file, carved_dir, carved_types, target_funcs = sys.argv[1:]
stem = Path(bc_file).stem

type_info, types = read_carved_func_type(carved_types)

funcs = set()
for fn in glob.glob("{}/*".format(carved_dir)):
    if "call_seq" in fn:
        continue

    fn = "_".join(fn.split("/")[-1].split("_")[:-2])

    funcs.add(fn)

print("# of funcs : {}".format(len(funcs)))

with open(target_funcs, "w") as f:
    for fn in funcs:
        f.write("{}\n".format(fn))

for funcname in type_info:
    funcs.add(funcname)


# llvm-cov
# -L /usr/lib/llvm-13/lib/clang/13.0.1/lib/linux/ -l:libclang_rt.profile-x86_64.a

with open("make_driver.sh", "w") as f:
    f.write("#!/bin/bash\n")
    idx = 0
    for fn in funcs:
        f.write(
            "{} {} {} -lssl -lcrypto -lz -lpthread -ldl &\n".format(
                unit_driver_pass_py, bc_file, fn
            )
        )
        idx += 1

        if idx == 10:
            f.write("wait\n")
            idx = 0
    f.write("wait\n")

with open("run_tests.sh", "w") as f:
    f.write("#!/bin/bash\n")
    idx = 0
    for fn in glob.glob("{}/*".format(carved_dir)):
        if "call_seq" in fn:
            continue
        func = "_".join(fn.split("/")[-1].split("_")[:-2])
        f.write("./{}.{}.driver {} &\n".format(stem, func, fn))

        idx += 1

        if idx == 10:
            f.write("wait\n")
            idx = 0

    f.write("wait\n")
