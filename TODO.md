Alive2 TODO list
================

Making counterexamples easier to read
-------------------------------------

- flag inputs whose value does not matter

- show control flow taken

- don't show dead values

- show where execution became undefined

- figure out which pass in the phase ordering broke it

- make CEXs relating to function call side effects easier to
  understand, currently they show up as memory state mismatches:
  http://volta.cs.utah.edu:8080/z/khXHQM

Usability improvements not specifically related to counterexamples
------------------------------------------------------------------

- (optionally) make a note in the output when an optimization is
  correct, but would not be correct under a different datalayout

- consider adding flags similar to --disable-undef-inputs, but for
  float values. perhaps: `--disable-nan-inputs`, `--disable-inf-inputs`,
  `--disable-negative-zero-inputs`
