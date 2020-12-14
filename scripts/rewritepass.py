#!/usr/bin/python3
import sys
import os

if len(sys.argv) != 3:
  print("Use: %s <PassRegistry.def path> <passes>" % sys.argv[0])
  exit(1)

passregpath = sys.argv[1]
passes = sys.argv[2].strip("'\"").split(',')
plen = len(passes)

levels = ["module", "cgscc", "function", "loop"]
level_ancestors = {
  "module":[],
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
Build the stack of pass managers by finding the first pass from PassRegistry.def
"""

firstp_level = None
# If the level is explicitly given, use it
for k in levels:
  if passes[0].startswith(k + "("):
    firstp_level = k
    break
if passes[0].startswith("loop-mssa("):
  firstp_level = "loop"

for i in ["s", "z", "0", "1", "2", "3"]:
  for pipeline in ["default", "thinlto-pre-link", "thinlto", "lto-pre-link",
                   "lto"]:
    if passes[0] == "%s<O%s>" % (pipeline, i):
      firstp_level = "module"
      break
  if firstp_level != None:
    break

# PassBuilder.cpp: isCGSCCPassName()
if passes[0].startswith("devirt<") or passes[0].startswith("repeat<"):
  firstp_level = "cgscc"

if firstp_level == None:
  # ask PassRegistry.def
  firstpass = passes[0]
  if passes[0].startswith("require<"):
    firstpass = passes[0][len("require<"):]
  firstpass = firstpass.replace("(", ",").replace("<",",").replace(">",",") \
                       .replace(";", ",").split(",")[0]

  # There are passes like "verify", which can be either function or module
  # level. We should allow them
  found_levels = dict()
  prefixes = ["MODULE_", "CGSCC_", "FUNCTION_", "LOOP_"]
  for l in open(passregpath, "r").readlines():
    if l.find('"%s"' % firstpass) == -1 and l.find('"%s<' % firstpass) == -1:
      continue
    for i in range(0, len(prefixes)):
      if l.startswith(prefixes[i]):
        found_levels[levels[i]] = True

  if "module" in found_levels and len(found_levels) == 1:
    # precisely a module level pass
    firstp_level = "module"
  else:
    for i in range(1, len(prefixes)): # can be non-module (ex: verify)
      if levels[i] in found_levels:
        firstp_level = levels[i]
        break

prefix = ""
suffix = ""

if firstp_level != "module":
  if not passes[0].startswith(firstp_level):
    # don't print "function(function(passes))"
    prefix = firstp_level + "("
    suffix = ")"

  for p in level_ancestors[firstp_level]:
    prefix = p + "(" + prefix
    suffix = suffix + ")"

print(firstp_level + " " + prefix + ",".join(passes) + suffix)
