#!/usr/bin/python3

import sys, os
import subprocess as sp


def check_given_bitcode(inputbc):
    # check given file exists
    if not os.path.isfile(inputbc):
        print("Can't find file : {}".format(inputbc))
        return False

    # check given file format
    cmd = ["file", inputbc]
    stdout = sp.run(cmd, stdout=sp.PIPE, stderr=sp.DEVNULL).stdout
    if b"bitcode" not in stdout:
        print("Can't recognize file : {}".format(inputbc))
        return False

    return True


def get_ld_path():
    cmd = ["llvm-config", "--bindir"]
    out = sp.run(cmd, stdout=sp.PIPE, stderr=sp.PIPE)

    if b"command not found" in out.stderr:
        print("Can't find llvm-config, please check PATH")
        exit()

    ld_path = out.stdout.decode()[:-1] + "/ld.lld"
    return ld_path


def read_carved_func_type(type_infof):
    type_info = dict()  # func_name -> [(var_name, type_name)]
    types = set()
    with open(type_infof, "r") as f1:
        funcname = ""
        for line in f1:
            if line.startswith("##"):
                funcname = line.strip()[2:]
            elif line.startswith("**"):
                line = line.strip().split(" : ")
                var_name = line[0][2:]
                type_name = line[1]
                if funcname not in type_info:
                    type_info[funcname] = []
                type_info[funcname].append((var_name, type_name))
                types.add(type_name)
            else:
                print("Wrong format!\n")
                break

    return type_info, types


def get_link_option(input_bc_filename):
    orig_filename = ".".join(input_bc_filename.split(".")[:-1])

    cmd = ["ldd", orig_filename]

    out = sp.run(cmd, stdout=sp.PIPE, stderr=sp.PIPE).stdout.decode()

    link_commands = []

    for line in out.split("\n"):
        if "linux-vdso.so" in line:
            continue
        if "libgcc_s.so" in line:
            continue
        if "ld-linux-x86-64.so" in line:
            continue
        if "libc.so" in line:
            continue

        if "=>" not in line:
            continue

        line = line.strip().split("=>")[0]

        if "lib" not in line and "so" not in line:
            continue

        so_name = line.split("lib")[1].split(".so")[0]
        link_commands.append("-l" + so_name)

    return link_commands


def get_clang_version():
    cmd = ["clang++", "--version"]
    out = sp.run(cmd, stdout=sp.PIPE, stderr=sp.PIPE).stdout.decode()
    tokens = out.split()
    version = tokens[tokens.index("version") + 1].split(".")[0]
    return version

def need_asan_flag(input_bc_filename):
    orig_filename = ".".join(input_bc_filename.split(".")[:-1])

    cmd = ["nm", orig_filename]
    out = sp.run(cmd, stdout=sp.PIPE, stderr=sp.PIPE).stdout.decode()

    return "__asan_report" in out
