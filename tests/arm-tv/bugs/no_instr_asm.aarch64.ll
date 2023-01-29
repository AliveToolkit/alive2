; TEST-ARGS: --disable-undef-input --disable-poison-input

define i9 @foo() {
  unreachable
}
