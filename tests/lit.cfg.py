import os
import lit.formats

config.name = 'Alive2'
config.test_format = lit.formats.Alive2Test()
config.test_source_root = os.path.dirname(__file__)

# JDR: temporary
config.excludes = ['unhandled']
