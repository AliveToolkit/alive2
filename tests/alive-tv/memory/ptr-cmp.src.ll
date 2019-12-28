; ERROR: Target is more poisonous

define i1 @f(i32** %a) {
  %load = load i32*, i32** %a
  %integral = ptrtoint i32* %load to i64
  %cmp = icmp slt i64 %integral, 0
  ret i1 %cmp
}
