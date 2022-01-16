define i17 @src(i17* %p) {
  %p2 = bitcast i17* %p to <2 x i17>*
  store <2 x i17> <i17 1, i17 2>, <2 x i17>* %p2
  %load = load i17, i17* %p
  ret i17 %load
}

define i17 @tgt(i17* %p) {
  %p2 = bitcast i17* %p to <2 x i17>*
  store <2 x i17> <i17 1, i17 2>, <2 x i17>* %p2
  ret i17 1
}
