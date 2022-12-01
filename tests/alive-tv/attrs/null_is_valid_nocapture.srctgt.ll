define i8 @src(i1 %C, ptr %P) null_pointer_is_valid {
  %P2 = select i1 %C, ptr %P, ptr null
  %V = load i8, ptr %P2
  ret i8 %V
}

define i8 @tgt(i1 %C, ptr nocapture %P) null_pointer_is_valid {
  %P2 = select i1 %C, ptr %P, ptr null
  %V = load i8, ptr %P2, align 1
  ret i8 %V
}
