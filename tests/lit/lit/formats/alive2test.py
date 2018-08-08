# Copyright (c) 2018-present The Alive2 Authors.
# MIT license that can be found in the LICENSE file.

import lit.TestRunner
import lit.util
from .base import FileBasedTest
import re, signal, string, subprocess


def executeCommand(command):
  p = subprocess.Popen(command,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
  out,err = p.communicate()
  exitCode = p.wait()

  # Detect Ctrl-C in subprocess.
  if exitCode == -signal.SIGINT:
    raise KeyboardInterrupt

  # Ensure the resulting output is always of string type.
  try:
    out = str(out.decode('ascii'))
  except:
    out = str(out)
  try:
    err = str(err.decode('ascii'))
  except:
    err = str(err)
  return out, err, exitCode


def readFile(path):
  fd = open(path, 'r')
  return fd.read()


class Alive2Test(FileBasedTest):
  def __init__(self):
    self.regex_errs = re.compile(r";\s*(ERROR:.*)")
    self.regex_args = re.compile(r";\s*TEST-ARGS:(.*)")

  def execute(self, test, litConfig):
    test = test.getSourcePath()
    cmd = ['./alive']
    input = readFile(test)

    # add test-specific args
    m = self.regex_args.search(input)
    if m != None:
      cmd += m.group(1).split()

    cmd.append(test)
    out, err, exitCode = executeCommand(cmd)

    m = self.regex_errs.search(input)
    if m == None:
      if exitCode == 0 and string.find(out, 'Optimization is correct!') != -1:
        return lit.Test.PASS, ''
      return lit.Test.FAIL, out + err

    if exitCode != 0 and string.find(err, m.group(1)) != -1:
      return lit.Test.PASS, ''
    return lit.Test.FAIL, out + err
