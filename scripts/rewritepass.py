#!/usr/bin/python3
import sys
import os

if len(sys.argv) != 3:
  print("Use: %s <opt path> <passes>" % sys.argv[0])
  exit(1)

optpath = sys.argv[1]
passes = sys.argv[2].strip("'\"").split(',')
plen = len(passes)

level_ancestors = {
  "cgscc":["module"],
  "function":["module"],
      # don't use module(cgscc(function(...)), use module(function(...))
      # the former one results in a different output.
      # e.g. llvm/test/Transforms/LoopVectorize/reduction-order.ll
  "loop":["function", "module"]
}

"""
PassBuilder.cpp states that:
  As a special shortcut, if the very first pass is not
  a module pass (as a module pass manager is), this will automatically form
  the shortest stack of pass managers that allow inserting that first pass.
Build the stack of pass managers by invoking opt with the first pass and
parsing its error message.
"""
i = 0
firstp = passes[i]
if firstp.find("<") != -1 or firstp.find("(") != -1:
  # make p complete by concatenating tokens until the corresponding ">"
  # and ")" are found
  par = par2 = 0
  ps = []
  while i < plen:
    ps.append(passes[i])
    par  += passes[i].count("(") - passes[i].count(")")
    par2 += passes[i].count("<") - passes[i].count(">")
    if par == 0 and par2 == 0:
      break
    i = i + 1
  firstp = ",".join(ps)


# 'opt -passes=..' does not generate the message we want if one of these passes
# is used.
unsupported_module_passes = [
  "function-import",
  "pgo-instr-use",
  "sample-profile"
]

firstp_level = "module"
if firstp not in unsupported_module_passes:
  # get the level of the first pass by invoking opt and parsing the output
  # message.
  optparam = '-passes="%s,module(globalopt)" -disable-output 2>&1' % firstp
  output = os.popen('echo "" | "%s" %s' %
                    (optpath, optparam)).read()
  lines = output.strip().split("\n")
  assert(len(lines) == 1)
  if lines[0] != "":
    msg = "invalid use of 'module' pass as ";
    assert(lines[0].find(msg) != -1), \
          "unknown output from opt: " + lines[0] + ", optparam: " + optparam

    # level_str becomes e.g. "function pipeline"
    level_str = lines[0][lines[0].find(msg) + len(msg):]
    # get "function"
    firstp_level = level_str.split()[0]
    assert(firstp_level in level_ancestors)
  else:
    firstp_level = "module"

i = i + 1
res = [firstp] + passes[i:]

prefix = ""
suffix = ""

if firstp_level != "module":
  if not firstp.startswith(firstp_level):
    # don't print "function(function(passes))"
    prefix = firstp_level + "("
    suffix = ")"

  for p in level_ancestors[firstp_level]:
    prefix = p + "(" + prefix
    suffix = suffix + ")"


print(firstp_level + " " + prefix + ",".join(res) + suffix)
