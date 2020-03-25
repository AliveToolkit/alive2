Alive2 TODO list
================

Making counterexamples easier to read:

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
