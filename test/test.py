import os
from pathlib import Path
import unittest

script_file_path = Path(os.path.realpath(__file__))
project_path = script_file_path.parent.parent

class CarvingIR(unittest.TestCase):
    def test_1_sample_foo_args(self):
        self.assertTrue(True)               # TODO: fill automated script

if __name__ == '__main__':
    unittest.main(verbosity=2)