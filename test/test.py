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


class CarvingIR(unittest.TestCase):
    def setUp(self):
        self.fp = tempfile.mkdtemp()
        self.temp_dir = Path(self.fp)
        self.carve_inputs = self.temp_dir / "carve_inputs"
        self.carve_inputs.mkdir()
        self.binary = self.temp_dir / "main"
        self.bitcode = self.temp_dir / "main.bc"
        self.carved_binary = self.temp_dir / "main.carv"
        return

    def tearDown(self):
        sp.run(["rm", "-rf", self.fp])
        pass

    def template(self, code, argv=[]):
        compiler = "gclang++" if code.suffix == ".cc" else "gclang"
        argv_string = ' '.join(argv)
        sp.run([compiler, code, "-O0", "-g", "-o", self.binary], stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        original_output = sp.run([self.binary, argv_string], stderr=sp.PIPE).stderr
        sp.run(["get-bc", "-o", self.bitcode, self.binary], stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        sp.run([carve_pass_bin, self.bitcode, "func_args"], cwd=self.temp_dir, stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        sp.run([self.carved_binary, argv_string, "carve_inputs"], cwd=self.temp_dir, stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        
        q = []
        processed_functions = set([])
        for x in self.carve_inputs.iterdir():
            name = x.name
            if name != "call_seq":
                if compiler == "gclang++":
                    if not name.startswith("_Z3"):
                        continue
                l1 = name.rfind('_')
                l2 = name.rfind('_', 0, l1)
                func =  name[:l2]
                n1 = name[l2+1:l1]
                n2 = name[l1+1:]
                q.append((n1, n2, func))
                if not func in processed_functions:
                    processed_functions.add(func)
                    sp.run([simple_unit_driver_bin, self.bitcode, func], stdout=sp.DEVNULL, stderr=sp.DEVNULL)
        q.sort()
        
        results = []
        for (n1, n2 ,func) in q:    
            driver = self.temp_dir / f"main.{func}.driver"
            # Only first line because f() may call g()
            replay_result = sp.run([driver, self.carve_inputs / f"{func}_{n1}_{n2}"], stderr=sp.PIPE).stderr.split(b'\n')[0] + b'\n'  
            results.append(replay_result)
        
        replay_output = b''.join(results)
        self.assertEqual(original_output, replay_output)
    
    def test_01_sample_args(self):
        source_code = project_path / "IR_example" / "1_simple" / "main.c"
        argv = ["1", "2", "3", "4", "5"]
        self.template(source_code, argv)
    
    def test_02_two_pointers(self):
        self.template(project_path / "IR_example" / "2_two_pointers" / "main.c")
    
    def test_03_struct(self):
        self.template(project_path / "IR_example" / "3_struct" / "main.c")
    
    
    def test_04_double_pointer(self):
        #self.template(project_path / "IR_example" / "4_double_pointer" / "main.c")
        pass
    
    def test_05_recursive_struct(self):
        self.template(project_path / "IR_example" / "5_recursive_struct" / "main.c")
    
    def test_06_global_var(self):
        self.template(project_path / "IR_example" / "6_global_var" / "main.c")
    
    def test_07_func_ptr(self):
        self.template(project_path / "IR_example" / "7_func_ptr" / "main.c")

    def test_08_cpp_vector(self):
        self.template(project_path / "IR_example" / "8_c++_vector" / "main.cc")

    def test_082_cpp_vector(self):
        self.template(project_path / "IR_example" / "8_2_c++_vector" / "main.cc")
    
    def test_09_cpp_map(self):
        self.template(project_path / "IR_example" / "9_c++_map" / "test.cc")
    
    def test_11_cpp_one_inheritance(self):
        self.template(project_path / "IR_example" / "11_c++_one_inheritance" / "test.cc")
    
    def test_12_cpp_virtual_method(self):
        self.template(project_path / "IR_example" / "12_c++_virtual_method" / "test.cc")


if __name__ == "__main__":
    unittest.main(verbosity=2)
