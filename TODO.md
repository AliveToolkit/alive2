Alive2 TODO list
================


teste
teste2

Making counterexamples easier to read
-------------------------------------

- don't use undef/poison in CEX unless necessary

- make integer values as simple as possible

- flag inputs whose value does not matter

- show control flow taken

- don't show dead values

- highlight the return value, if it differs between src and tgt

- show where execution became undefined

- figure out which pass in the phase ordering broke it

- we'd like a reducer for IR, perhaps a C-Reduce hack if bugpoint
  isn't good enough

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
