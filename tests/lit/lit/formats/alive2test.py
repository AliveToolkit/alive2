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

def is_timeout(str):
  return str.find('ERROR: Timeout') > 0

def id_check(fn, cmd, args):
  out, err, exitCode = executeCommand(cmd + args + ["-always-verify"])
  str = out + err
  if not is_timeout(str) and (exitCode != 0 or str.find(ok_string) < 0):
    raise Exception(fn + ' identity check fail: ' + str)


def readFile(path):
  fd = open(path, 'r')
  return fd.read()


class Alive2Test(TestFormat):
  def __init__(self):
    self.regex_errs = re.compile(r";\s*(ERROR:.*)")
    self.regex_xfail = re.compile(r";\s*XFAIL:\s*(.*)")
    self.regex_args = re.compile(r"(?:;|//)\s*TEST-ARGS:(.*)")
    self.regex_check = re.compile(r"(?:;|//)\s*CHECK:(.*)")
    self.regex_check_not = re.compile(r"(?:;|//)\s*CHECK-NOT:(.*)")
    self.regex_skip_identity = re.compile(r";\s*SKIP-IDENTITY")
    self.regex_errs_out = re.compile("ERROR:.*")

  def getTestsInDirectory(self, testSuite, path_in_suite,
                          litConfig, localConfig):
    source_path = testSuite.getSourcePath(path_in_suite)
    for filename in os.listdir(source_path):
      filepath = os.path.join(source_path, filename)
      if not filename.startswith('.') and \
          not os.path.isdir(filepath) and \
          (filename.endswith('.opt') or filename.endswith('.src.ll') or
           filename.endswith('.srctgt.ll') or filename.endswith('.c') or
           filename.endswith('.cpp') or filename.endswith('.opt.ll') or
           filename.endswith('.ident.ll')):
        yield lit.Test.Test(testSuite, path_in_suite + (filename,), localConfig)


  def execute(self, test, litConfig):
    test = test.getSourcePath()

    alive_tv_1 = test.endswith('.srctgt.ll')
    alive_tv_2 = test.endswith('.src.ll')
    alive_tv_3 = test.endswith('.ident.ll')
    if alive_tv_1 or alive_tv_2 or alive_tv_3:
      cmd = ['./alive-tv', '-smt-to=20000']
      if not os.path.isfile('alive-tv'):
        return lit.Test.UNSUPPORTED, ''

    opt_tv = test.endswith('.opt.ll')
    if opt_tv:
      cmd = ['./opt-alive-test.sh', '-disable-output']
      if not os.path.isfile('opt-alive-test.sh'):
        return lit.Test.UNSUPPORTED, ''

    clang_tv = test.endswith('.c') or test.endswith('.cpp')
    if clang_tv:
      execpath = './%s' % ("alivecc" if test.endswith('.c')
                                     else "alive++")
      # 30 seconds is too long to apply to all passes, just use the default to
      cmd = [execpath, "-c", "-o", "/dev/null"]
      if not os.path.isfile(execpath):
        return lit.Test.UNSUPPORTED, ''

    if not alive_tv_1 and not alive_tv_2 and not alive_tv_3 and \
       not clang_tv and not opt_tv:
      cmd = ['./alive', '-smt-to:20000']

    input = readFile(test)

    # add test-specific args
    m = self.regex_args.search(input)
    if m != None:
      cmd += m.group(1).split()

    do_identity = self.regex_skip_identity.search(input) is None

    # Run identity check first
    if alive_tv_1 and do_identity:
      try:
        id_check('src', cmd, [test, '-src-fn=src', '-tgt-fn=src'])
        id_check('tgt', cmd, [test, '-src-fn=tgt', '-tgt-fn=tgt'])
      except Exception as e:
        return lit.Test.FAIL, e

    if alive_tv_2 and do_identity:
      try:
        id_check('src', cmd, [test, test])
        tgtpath = test.replace('.src.ll', '.tgt.ll')
        id_check('tgt', cmd, [tgtpath, tgtpath])
      except Exception as e:
        return lit.Test.FAIL, e

    cmd.append(test)
    if alive_tv_2:
      cmd.append(test.replace('.src.ll', '.tgt.ll'))
    elif alive_tv_3:
      cmd = cmd + [test, '-always-verify']
    out, err, exitCode = executeCommand(cmd)
    output = out + err

    xfail = self.regex_xfail.search(input)
    if xfail != None and output.find(xfail.group(1)) != -1:
      return lit.Test.XFAIL, ''

    if is_timeout(output):
      return lit.Test.PASS, ''

    # allow multiple 'CHECK: ..'
    chks = self.regex_check.findall(input)
    for chk in chks:
      if output.find(chk.strip()) == -1:
        return lit.Test.FAIL, output

    chk_not = self.regex_check_not.search(input)
    if chk_not != None and output.find(chk_not.group(1).strip()) != -1:
      return lit.Test.FAIL, output

    if clang_tv and exitCode != 0:
      # clang tv should not exit with non-zero even if validation fails.
      # Otherwise it will stop a build system such as `make`.
      return lit.Test.FAIL, output

    expect_err = self.regex_errs.search(input)
    if expect_err is None and xfail is None and len(chks) == 0 and \
       chk_not is None:
      # If there's no other test, correctness of the transformation should be
      # checked.
      if exitCode == 0 and output.find(ok_string) != -1 and \
          self.regex_errs_out.search(output) is None:
        return lit.Test.PASS, ''
      return lit.Test.FAIL, output

    if expect_err != None and output.find(expect_err.group(1)) == -1:
      return lit.Test.FAIL, output

    return lit.Test.PASS, ''
