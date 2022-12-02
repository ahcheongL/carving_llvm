#!/usr/bin/python3

import sys
import argparse
import struct
import os

OBJTYPES = ["CHAR", "SHORT", "INT", "LONG", "LONGLONG", "FLOAT", "DOUBLE",
"LONGDOUBLE", "PTR", "NULL", "FUNCPTR", "UNKNOWN_PTR"]

class CarvedObj:
    def __init__(self, type, value, name = "", offset = 0):
        self.name = name
        self.type = type
        self.value = value
        self.offset = offset    

    def __str__(self):
        if self.type == 8:
            return "%s %s %s %s" % (self.name, OBJTYPES[self.type], self.value, self.offset)
        return "%s %s %s" % (self.name, OBJTYPES[self.type], self.value)

class CarvedPtr:
    def __init__(self, addr, size, typename = ""):
        self.ptr_addr = addr
        self.size = size
        self.type = typename
    
    def __str__(self):
        return "%s %d %s" % (self.ptr_addr, self.size, self.type)

class CarvedContext:
    def __init__(self):
        self.carved_ptrs = []
        self.carved = []
    
    def __repr__(self) -> str:
        return f"CarvedContext: # of ptrs : {len(self.carved_ptrs)}, # of objs : {len(self.carved)}"


def get_type_bytes_len(type):
    typename = OBJTYPES[type]
    if typename == "CHAR":
        return 1
    if typename == "SHORT":
        return 2
    if typename == "INT":
        return 4
    if typename == "LONG":
        return 8
    if typename == "LONGLONG":
        return 8
    if typename == "FLOAT":
        return 4
    if typename == "DOUBLE":
        return 8
    if typename == "PTR":
        return 4
    if typename == "NULL":
        return 4
    if typename == "UNKNOWN_PTR":
        return 4
    if typename == "FUNCPTR":
        return 4
    

def parse_txt(inputfn):
    carved_ctx = CarvedContext()
    inputf = open(inputfn, "r")
    for line in inputf:
        line = line.strip()
        if line == "####":
            break

        line = line.split(":")
        assert(len(line) == 4)

        newptr = CarvedPtr(line[1], int(line[2]), line[3])
        carved_ctx.carved_ptrs.append(newptr)
    
    for line in inputf:
        line = line.strip().split(":")

        try:
          assert(line[1] in OBJTYPES)
        except Exception as e:
            print(e)
            print(line)
            exit(0)

        typeidx = OBJTYPES.index(line[1])

        if len(line) == 3:
            if typeidx == 9 or typeidx == 11:
                newobj = CarvedObj(typeidx, -1, line[0])
            elif typeidx == 5 or typeidx == 6:
                float_num = float(line[2])
                int_num = struct.pack('<f', float_num)

                print("float_num : {}, int_num : {}", float_num, int_num)

                newobj = CarvedObj(typeidx, int_num, line[0])
            else:
                newobj = CarvedObj(typeidx, int(line[2]), line[0])
        else:
            newobj = CarvedObj(typeidx, int(line[2]), line[0], int(line[3]))
        
        carved_ctx.carved.append(newobj)

    inputf.close()

    return carved_ctx

def parse_bin(inputfn): #deprecated
    carved_ctx = CarvedContext()
    inputf = open(inputfn, "rb")

    num_ptrs = int.from_bytes(inputf.read(4), byteorder='little')
    for _i in range(num_ptrs):
        ptr_addr = hex(int.from_bytes(inputf.read(8), byteorder='little'))
        size = int.from_bytes(inputf.read(4), byteorder='little')
        newptr = CarvedPtr(ptr_addr, size)
        carved_ctx.carved_ptrs.append(newptr)
    
    num_objs = int.from_bytes(inputf.read(4), byteorder='little')
    for _i in range(num_objs):
        type = int.from_bytes(inputf.read(1), byteorder='little') % len(OBJTYPES)
        value_len = get_type_bytes_len(type)

        if value_len > 0:
            value = int.from_bytes(inputf.read(value_len), byteorder='little')
        else :
            value = 0

        if type == 8:
            offset = int.from_bytes(inputf.read(4), byteorder='little')
        else :
            offset = 0

        newobj = CarvedObj(type, value, "", offset)
        carved_ctx.carved.append(newobj)

    inputf.close()
    return carved_ctx

def write_bin(outpufn, carved_ctx):
    outputf = open(outpufn, "wb")
    outputf.write(len(carved_ctx.carved_ptrs).to_bytes(4, byteorder='little'))
    for ptr in carved_ctx.carved_ptrs:
        outputf.write(ptr.size.to_bytes(4, byteorder='little'))
    
    for obj in carved_ctx.carved:
        value_len = get_type_bytes_len(obj.type)
        if value_len > 0:
            try:
              outputf.write(obj.value.to_bytes(value_len, byteorder='little', signed=True))
            except Exception as e:
                print(e)
                print(obj)
                print(value_len)
                exit(0)
        if obj.type == 8:
            outputf.write(obj.offset.to_bytes(4, byteorder='little'))
    
    outputf.close()

def write_txt(outputfn, carved_ctx):
    outputf = open(outputfn, "w")
    ptridx = 0
    for ptr in carved_ctx.carved_ptrs:
        outputf.write("%d:%s:%d:%s\n" % (ptridx, ptr.ptr_addr, ptr.size, ptr.type))
    outputf.write("####\n")
    for obj in carved_ctx.carved:
        if obj.type == 8:
            outputf.write("%s:%s:%s:%d\n" % (obj.name, OBJTYPES[obj.type], obj.value, obj.offset))
        else:
            outputf.write("%s:%s:%s\n" % (obj.name, OBJTYPES[obj.type], obj.value))
    outputf.close()


def convert_one_file(inputfn, outputfn):
    carved_ctx = parse_txt(inputfn)        
    write_bin(outputfn, carved_ctx)

def convert_directory(inputdir, outputdir, out_format):
    for filename in os.listdir(inputdir):
        inputfn = os.path.join(inputdir, filename)
        outputfn = os.path.join(outputdir, filename)
        convert_one_file(inputfn, outputfn, out_format)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--input-directory', '--id', nargs=1, help='input directory')
    parser.add_argument('--output-directory', '--od', nargs=1, help='output directory')
    parser.add_argument('--input', '-i', nargs=1, help='input file')
    parser.add_argument('--output', '-o', nargs=1, help='output file')
    args = parser.parse_args()

    if (args.input and args.input_directory) or (args.output and args.output_directory):
        print('Error: input and output file and directory cannot be used at the same time')
        sys.exit(1)
    
    if not (args.input and args.output) and not (args.input_directory and args.output_directory):
        print('Error: input and output file or directory must be specified')
        sys.exit(1)

    if args.input:
        convert_one_file(args.input[0], args.output[0])
    
    else:
        convert_directory(args.input_directory[0], args.output_directory[0])
