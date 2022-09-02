import os
from pathlib import Path
import unittest
import tempfile
import subprocess

script_file_path = Path(os.path.realpath(__file__))
project_path = script_file_path.parent.parent
carve_pass_bin = project_path.joinpath("bin", "carve_pass.py")
simple_unit_driver_bin = project_path.joinpath("bin", "simple_unit_driver_pass.py")


def run_gdb(executable, args, gdb_script):

    output = subprocess.check_output(
        [
            "gdb",
            "-q",
            "--command={}".format(gdb_script),
            "--args",
            executable,
            args,
        ]
    )
    return list(
        map(
            lambda x: int(x[x.find(b"=") + 2 :]),
            filter(
                lambda x: x.startswith(b"$"),
                output.split(b"\n"),
            ),
        )
    )


class CarvingIR(unittest.TestCase):
    def test_1_sample_args(self):
        with tempfile.TemporaryDirectory() as fp:
            func = "foo"
            input_args = "1 2 3 4 5"
            source_code = project_path.joinpath("IR_example", "1_simple", "main.c")
            temp_dir = Path(fp)
            carve_inputs_dir = temp_dir.joinpath("carve_inputs")
            carve_inputs_dir.mkdir()
            binary = temp_dir.joinpath("main")
            bitcode = temp_dir.joinpath("main.bc")
            carved_binary = temp_dir.joinpath("main.carv")
            gdb_script = project_path.joinpath(
                "test", "gdb_scripts", "1_simple", "foo.txt"
            )
            driver = temp_dir.joinpath("main.foo.driver")
            driver_input = carve_inputs_dir.joinpath("foo_1_0")
            subprocess.run(["gclang", source_code, "-O0", "-o", binary])
            subprocess.run(["get-bc", "-o", bitcode, binary])
            subprocess.run([carve_pass_bin, bitcode, "func_args"])
            subprocess.run([carved_binary, input_args, "carve_inputs"], cwd=temp_dir)
            subprocess.run([simple_unit_driver_bin, bitcode, func])
            original_gdb_output = run_gdb(binary, input_args, gdb_script)
            replay_gdb_output = run_gdb(driver, driver_input, gdb_script)
            self.assertEqual(original_gdb_output, replay_gdb_output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
