define fp128 @src(fp128 %a) {
  %cmp = fcmp ogt fp128 %a, 0xL40008000000000000000000000000000
  br i1 %cmp, label %then, label %else

then:
  ret fp128 %a

else:
  ret fp128 0xLA0008000000000000000000000000000
}
