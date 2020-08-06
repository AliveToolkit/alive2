# Copyright (c) 2018-present The Alive2 Authors.
# MIT license that can be found in the LICENSE file.

import lit.TestRunner
import lit.util
from .base import TestFormat
import os, re, signal, string, subprocess

ok_string = 'Transformation seems to be correct!'

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


def id_check(fn, cmd, args):
  out, err, exitCode = executeCommand(cmd + args)
  if exitCode != 0 or (out + err).find(ok_string) < 0:
    raise Exception(fn + ' identity check fail: ' + out + err)


def readFile(path):
  fd = open(path, 'r')
  return fd.read()


class Alive2Test(TestFormat):
  def __init__(self):
    self.regex_errs = re.compile(r";\s*(ERROR:.*)")
    self.regex_xfail = re.compile(r";\s*XFAIL:\s*(.*)")
    self.regex_args = re.compile(r";\s*TEST-ARGS:(.*)")
    self.regex_check = re.compile(r";\s*CHECK:(.*)")
    self.regex_check_not = re.compile(r";\s*CHECK-NOT:(.*)")
    self.regex_errs_out = re.compile("ERROR:.*")

  def getTestsInDirectory(self, testSuite, path_in_suite,
                          litConfig, localConfig):
    source_path = testSuite.getSourcePath(path_in_suite)
    for filename in os.listdir(source_path):
      filepath = os.path.join(source_path, filename)
      if not filename.startswith('.') and \
          not os.path.isdir(filepath) and \
          (filename.endswith('.opt') or filename.endswith('.src.ll') or
           filename.endswith('.srctgt.ll')):
        yield lit.Test.Test(testSuite, path_in_suite + (filename,), localConfig)


  def execute(self, test, litConfig):
    test = test.getSourcePath()

    alive_tv_1 = test.endswith('.srctgt.ll')
    if alive_tv_1:
      cmd = ['./alive-tv', '-smt-to=20000']
      if not os.path.isfile('alive-tv'):
        return lit.Test.UNSUPPORTED, ''

    alive_tv_2 = test.endswith('.src.ll')
    if alive_tv_2:
      cmd = ['./alive-tv', '-smt-to=20000']
      if not os.path.isfile('alive-tv'):
        return lit.Test.UNSUPPORTED, ''

    if not alive_tv_1 and not alive_tv_2:
      cmd = ['./alive', '-smt-to:20000']

    input = readFile(test)

    # add test-specific args
    m = self.regex_args.search(input)
    if m != None:
      cmd += m.group(1).split()

    # Run identity check first
    if alive_tv_1:
      try:
        id_check('src', cmd, [test, '-src-fn=src', '-tgt-fn=src'])
        id_check('tgt', cmd, [test, '-src-fn=tgt', '-tgt-fn=tgt'])
      except Exception as e:
        return lit.Test.FAIL, e

    if alive_tv_2:
      try:
        id_check('src', cmd, [test, test])
        tgtpath = test.replace('.src.ll', '.tgt.ll')
        id_check('tgt', cmd, [tgtpath, tgtpath])
      except Exception as e:
        return lit.Test.FAIL, e

    cmd.append(test)
    if alive_tv_2:
      cmd.append(test.replace('.src.ll', '.tgt.ll'))
    out, err, exitCode = executeCommand(cmd)

    expect_err = self.regex_errs.search(input)
    xfail = self.regex_xfail.search(input)
    chk = self.regex_check.search(input)
    chk_not = self.regex_check_not.search(input)

    # Check XFAIL early.
    if xfail != None and (out + err).find(xfail.group(1)) != -1:
      return lit.Test.XFAIL, ''

    if chk != None and (out + err).find(chk.group(1).strip()) == -1:
      return lit.Test.FAIL, out + err

    if chk_not != None and (out + err).find(chk_not.group(1).strip()) != -1:
      return lit.Test.FAIL, out + err

    if expect_err is None and xfail is None and chk is None and chk_not is None:
      # If there's no other test, correctness of the transformation should be
      # checked.
      if exitCode == 0 and (out + err).find(ok_string) != -1 and \
          self.regex_errs_out.search(out + err) is None:
        return lit.Test.PASS, ''
      return lit.Test.FAIL, out + err

    if expect_err != None and (out + err).find(expect_err.group(1)) == -1:
      return lit.Test.FAIL, out + err

    return lit.Test.PASS, ''
