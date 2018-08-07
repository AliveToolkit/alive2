import os
import sys
import lit.formats

config.name = 'Alive2'

config.test_format = lit.formats.Alive2Test()
config.suffixes = ['.opt']
config.test_source_root = os.path.dirname(__file__)
