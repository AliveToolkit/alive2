Solver efficiency testing
=========================

The `test.pl` script is intended to evaluate how well some particular
version of Alive-tv works together with some particular version of
Z3. The strategy is to have a collection of test cases that are
parameterized by a value and then to see how high this value can be
pushed without triggering a solver timeout.

The M4 macro processor is used to replace every occurrence of `X` in a
test case with the value of the parameter. Since m4 considers `iX` to
be a single token, we use a special case to replace, for example, `iX`
with `i32` when the value of X is 32.

Special care should be taken when an open parenthesis follows an m4
macro; if no spaces are left between these two things, the
parenthesized text is considered by m4 to be a macro
argument. Therefore, this bitwidth-polymorphic declaration of LLVM's
ctpop intrinsic will not work:

```
declare iX @llvm.ctpop.iX(iX)
```

Rather, it should be written like this:

```
declare iX @llvm.ctpop.iX (iX)
```

All test cases in the "positive" directory will be considered to fail
unless alive-tv believes that @tgt refines @src. Test cases in the
"negative" directory are expected to be refinement failures.

To ensure stable results, this script should always be run on an
otherwise idle machine.

