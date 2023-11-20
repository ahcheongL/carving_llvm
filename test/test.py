import os
from pathlib import Path
import unittest
import tempfile
import subprocess as sp
import heapq

script_file_path = Path(os.path.realpath(__file__))
project_path = script_file_path.parent.parent
carve_pass_bin = project_path / "bin" / "carve_pass.py"
simple_unit_driver_bin = project_path / "bin" / "simple_unit_driver_pass.py"

debug_level = 2

class CarvingIR(unittest.TestCase):
    # def setUp(self):
    #     self.fp = tempfile.mkdtemp()
    #     self.temp_dir = Path(self.fp)

    #     if (debug_level >= 2):
    #         print(f"Running directory: {self.temp_dir}")

    #     self.carve_inputs = self.temp_dir / "carve_inputs"
    #     self.carve_inputs.mkdir()

    #     self.binary = self.temp_dir / "main"
    #     self.bitcode = self.temp_dir / "main.bc"
    #     self.carved_binary = self.temp_dir / "main.carv"

    #     return

    # def tearDown(self):
    #     # sp.run(["rm", "-rf", self.fp])
    #     pass

    def template(self, code, argv=[]):
        self.result_path = project_path / "test" / "results"
        if not self.result_path.exists():
            self.result_path.mkdir()
        
        self.result_single_path = self.result_path / str(code.parent).split('/')[-1]
        if not self.result_single_path.exists():
            self.result_single_path.mkdir()
        
        sp.run(["cp", code, self.result_single_path])
        
        self.carve_inputs = self.result_single_path / "carve_inputs"
        if not self.carve_inputs.exists():
            self.carve_inputs.mkdir()
        
        self.unit_test_cases = self.result_single_path / "unit_test_cases"
        self.unit_test_cases_fp = open(self.unit_test_cases, 'w')

        self.replay_out = self.result_single_path / "replay_out"
        self.replay_out_fp = open(self.replay_out, 'w')
        
        self.binary = self.result_single_path / "main"
        self.bitcode = self.result_single_path / "main.bc"
        self.carved_binary = self.result_single_path / "main.carv"

        compiler = "gclang++" if code.suffix == ".cc" else "gclang"

        sp.run([compiler, code, "-O0", "-g", "-o", self.binary], stdout=sp.DEVNULL, stderr=sp.DEVNULL)

        original_output = sp.run([self.binary]+argv, stderr=sp.PIPE).stderr

        sp.run(["get-bc", "-o", self.bitcode, self.binary], stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        sp.run(["llvm-dis", self.bitcode], cwd=self.result_single_path, stdout=sp.DEVNULL, stderr=sp.DEVNULL)

        sp.run([carve_pass_bin, self.bitcode, "func_args"], cwd=self.result_single_path, stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        sp.run([self.carved_binary]+argv+["carve_inputs"], cwd=self.result_single_path, stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        
        q = []
        processed_functions = set([])
        for x in self.carve_inputs.iterdir():
            carved_name = x.name
            if carved_name != "call_seq":

                # if compiler == "gclang++":
                #     if not carved_name.startswith("_Z3"):
                #         continue

                l1 = carved_name.rfind('_')
                l2 = carved_name.rfind('_', 0, l1)

                carved_func = carved_name[:l2]

                func_name = carved_name[l2+1:l1]
                call_num = carved_name[l1+1:]

                q.append((func_name, call_num, carved_func))

                # make unit test driver for function
                if not func_name in processed_functions:
                    processed_functions.add(func_name)
                    sp.run([simple_unit_driver_bin, self.bitcode, func_name], stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        q.sort()

        # record carved inputs (unit test cases)
        for i in range(len(q)):
            inp = str(i) + ": " + str(q[i]) + "\n"
            self.unit_test_cases_fp.write(inp)

        
        results = []
        for (func_name, call_num, carved_func) in q:    
            driver = self.result_single_path / f"main.{func_name}.driver"

            # Only first line because f() may call g()
            replay_result = sp.run([driver, self.carve_inputs / f"{carved_func}{call_num}"], stderr=sp.PIPE).stderr
            self.replay_out_fp.write(str(driver) + ": " + carved_func+call_num + "\n")
            self.replay_out_fp.write(replay_result.decode())
            self.replay_out_fp.write("====================================\n\n")

        self.unit_test_cases_fp.close()
        self.replay_out_fp.close()
        
        # replay_output = b''.join(results)
        # self.assertEqual(original_output, replay_output)

    def test_00_yang(self):
        source_code = project_path / "test" / "yang" / "00" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)

    def test_01_yang(self):
        source_code = project_path / "test" / "yang" / "01" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)

    def test_02_yang(self):
        source_code = project_path / "test" / "yang" / "02" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)

    def test_03_yang(self):
        source_code = project_path / "test" / "yang" / "03" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)

    def test_04_yang(self):
        source_code = project_path / "test" / "yang" / "04" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)

    def test_05_yang(self):
        source_code = project_path / "test" / "yang" / "05" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)
    
    def test_06_yang(self):
        source_code = project_path / "test" / "yang" / "06" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)

    def test_07_yang(self):
        source_code = project_path / "test" / "yang" / "07" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)

    def test_08_yang(self):
        source_code = project_path / "test" / "yang" / "08" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)
    
    def test_01_sample_args(self):
        source_code = project_path / "IR_example" / "1_simple" / "test.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)
    
    def test_02_two_pointers(self):
        self.template(project_path / "IR_example" / "2_two_pointers" / "test.c")
    
    def test_03_struct(self):
        self.template(project_path / "IR_example" / "3_struct" / "main.c")
    
    def test_04_double_pointer(self):
        self.template(project_path / "IR_example" / "4_double_pointer" / "main.c")
    
    def test_05_recursive_struct(self):
        self.template(project_path / "IR_example" / "5_recursive_struct" / "main.c")
    
    def test_06_global_var(self):
        self.template(project_path / "IR_example" / "6_global_var" / "main.c")
    
    def test_07_func_ptr(self):
        self.template(project_path / "IR_example" / "7_func_ptr" / "main.c")

    # def test_08_cpp_vector(self):
    #     self.template(project_path / "IR_example" / "8_c++_vector" / "main.cc")

    # def test_082_cpp_vector(self):
    #     self.template(project_path / "IR_example" / "8_2_c++_vector" / "main.cc")
    
    # def test_09_cpp_map(self):
    #     self.template(project_path / "IR_example" / "9_c++_map" / "test.cc")
    
    # def test_11_cpp_one_inheritance(self):
    #     self.template(project_path / "IR_example" / "11_c++_one_inheritance" / "test.cc")
    
    # def test_12_cpp_virtual_method(self):
    #     self.template(project_path / "IR_example" / "12_c++_virtual_method" / "test.cc")
    
    # def test_27_cpp_class_arr(self):
    #     self.template(project_path / "IR_example" / "27_c++_class_arr" / "test.cc")


if __name__ == "__main__":
    unittest.main(verbosity=2)
