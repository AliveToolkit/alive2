define i1 @src(i32** %a) {
  %load = load i32*, i32** %a
  %integral = ptrtoint i32* %load to i64
  %cmp = icmp slt i64 %integral, 0
  ret i1 %cmp
}

define i1 @tgt(i32** %a) {
  %load = load i32*, i32** %a, align 8
  %cmp = icmp slt i32* %load, null
  ret i1 %cmp
}

