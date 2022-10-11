import os
from pathlib import Path
import unittest
import tempfile
import subprocess as sp

script_file_path = Path(os.path.realpath(__file__))
project_path = script_file_path.parent.parent
carve_pass_bin = project_path.joinpath("bin", "carve_pass.py")
simple_unit_driver_bin = project_path.joinpath("bin", "simple_unit_driver_pass.py")


def run_gdb(executable: Path, args: str, gdb_script_path: Path):

    output = sp.check_output(
        [
            "gdb",
            "-q",
            "--command={}".format(gdb_script_path),
            "--args",
            executable,
            args,
        ]
    ).split(b"\n")
    gdb_script = gdb_script_path.read_bytes().split(b"\n")

    for o in output:
        print(o.decode('utf-8'))
    query_result = []
    output_index = 1  # first line is ignored
    for query in gdb_script:
        #print(query, output[output_index])
        if query.startswith(b"b"):
            output_index += 1  # breakpoint takes one line
        elif query.startswith(b"r") or query.startswith(b"c"):
            output_index += 2  # run takes two line (breakpoint)
        elif query.startswith(b"p"):
            query_output = output[output_index]
            # print(query_output)
            query_result.append(int(query_output[query_output.find(b"=") + 2 :]))
            output_index += 1
        elif query[0:3] == b"x/d":
            query_output = output[output_index]
            query_result.append(int(query_output[query_output.find(b":") + 2 :]))
            output_index += 1
        elif query == b"quit":
            continue
        else:
            assert False  # Not handled case in gdb script

    return query_result


class CarvingIR(unittest.TestCase):
    def test_1_sample_args(self):
        with tempfile.TemporaryDirectory() as fp:
            input_args = "1 2 3 4 5"
            source_code = project_path.joinpath("IR_example", "1_simple", "main.c")
            temp_dir = Path(fp)
            gdb_script_dir = project_path.joinpath("test", "gdb_scripts", "1_simple")
            carve_inputs_dir = temp_dir.joinpath("carve_inputs")
            carve_inputs_dir.mkdir()
            binary = temp_dir.joinpath("main")
            bitcode = temp_dir.joinpath("main.bc")
            carved_binary = temp_dir.joinpath("main.carv")
            gdb_foo = gdb_script_dir.joinpath("foo.txt")
            gdb_goo_original = gdb_script_dir.joinpath("goo_original.txt")
            gdb_goo_replay = gdb_script_dir.joinpath("goo_replay.txt")
            driver_foo = temp_dir.joinpath("main.foo.driver")
            driver_foo_input = carve_inputs_dir.joinpath("foo_1_0")
            driver_goo = temp_dir.joinpath("main.goo.driver")
            driver_goo_input_1 = carve_inputs_dir.joinpath("goo_2_0")
            driver_goo_input_2 = carve_inputs_dir.joinpath("goo_3_1")
            sp.run(["gclang", source_code, "-O0", "-o", binary])
            sp.run(["get-bc", "-o", bitcode, binary])
            sp.run([carve_pass_bin, bitcode, "func_args"], cwd=temp_dir)
            sp.run([carved_binary, input_args, "carve_inputs"], cwd=temp_dir)
            sp.run([simple_unit_driver_bin, bitcode, "foo"])
            sp.run([simple_unit_driver_bin, bitcode, "goo"])
            original_gdb_output_foo = run_gdb(binary, input_args, gdb_foo)
            replay_gdb_output_foo = run_gdb(driver_foo, driver_foo_input, gdb_foo)
            self.assertEqual(original_gdb_output_foo, replay_gdb_output_foo)
            
            original_gdb_output_goo = run_gdb(binary, input_args, gdb_goo_original)
            replay_gdb_output_goo = run_gdb(
                driver_goo, driver_goo_input_1, gdb_goo_replay
            ) + run_gdb(driver_goo, driver_goo_input_2, gdb_goo_replay)
            
            self.assertEqual(original_gdb_output_goo, replay_gdb_output_goo)
    
    def test_8_2_cpp_vector(self):
        with tempfile.TemporaryDirectory() as fp:
            source_code = project_path / "IR_example" / "8_2_c++_vector" / "main.cc"
            temp_dir = Path(fp)
            carve_inputs = temp_dir / "carve_inputs"
            carve_inputs.mkdir()
            binary = temp_dir / "main"
            bitcode = temp_dir / "main.bc"
            carved_binary = temp_dir / "main.carv"
            driver_foo = temp_dir / "main._Z3fooSt6vectorI5ShapeSaIS0_EE.driver"
            input1 = carve_inputs / "_Z3fooSt6vectorI5ShapeSaIS0_EE_200_0"
            input2 = carve_inputs / "_Z3fooSt6vectorI5ShapeSaIS0_EE_309_1"
            sp.run(["gclang++", source_code, "-O0", "-g", "-o", binary])
            sp.run(["get-bc", "-o", bitcode, binary])
            sp.run([carve_pass_bin, bitcode, "func_args"], cwd=temp_dir)
            sp.run([carved_binary, "carve_inputs"], cwd=temp_dir)
            sp.run([simple_unit_driver_bin, bitcode, "_Z3fooSt6vectorI5ShapeSaIS0_EE"])
            original_output = sp.check_output([binary]).strip().split(b'\n')
            replay1 = sp.check_output([driver_foo, input1]).strip()
            replay2 = sp.check_output([driver_foo, input2]).strip()
            self.assertEqual(original_output[0], replay1)
            self.assertEqual(original_output[1], replay2)
            


if __name__ == "__main__":
    unittest.main(verbosity=2)
