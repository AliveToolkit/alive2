define i32 @src() {
  ret i32 5
}
define i32 @tgt() {
entry:
  %vs = call i32 @llvm.vscale.i32()
  %len = mul i32 %vs, 2
  %init_vec = insertelement <vscale x 2 x i32> poison, i32 0, i32 0
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %vec = phi <vscale x 2 x i32> [ %init_vec, %entry ], [ %new_vec, %for.body ]
  %cmp = icmp slt i32 %i, %len
  br i1 %cmp, label %for.body, label %exit

for.body:
  %new_vec = insertelement <vscale x 2 x i32> %vec, i32 %i, i32 %i
  %inc = add i32 %i, 1
  br label %for.cond

exit:
  %last_idx = sub i32 %len, 1
  %result = extractelement <vscale x 2 x i32> %vec, i32 %last_idx
  ret i32 %result
}

; TEST-ARGS: --vscale=3 -tgt-unroll=6
