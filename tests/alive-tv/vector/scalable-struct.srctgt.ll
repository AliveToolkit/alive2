; TEST-ARGS: -disable-undef-input

define {<vscale x 2 x i32>, i32} @src({<vscale x 2 x i32>, i32} %x) {
  ret {<vscale x 2 x i32>, i32} %x
}

define {<vscale x 2 x i32>, i32} @tgt({<vscale x 2 x i32>, i32} %x) {
  ret {<vscale x 2 x i32>, i32} %x
}
