define i1 @f(i32** %a) {
  %load = load i32*, i32** %a, align 8
  %cmp = icmp slt i32* %load, null
  ret i1 %cmp
}

