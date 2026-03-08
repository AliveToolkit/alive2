; TEST-ARGS: --src-unroll=2 --disable-undef-input
; SKIP-IDENTITY

define void @src(ptr %p, i1 %c) {
entry:
  br label %loop

loop:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %gep = getelementptr half, ptr %p, i32 %iv
  %x = load half, ptr %gep
  br i1 %c, label %then, label %latch

then:
  br label %latch

latch:
  %phi = phi nnan half [ %x, %loop ], [ 0.0, %then ]
  store half %phi, ptr %gep
  %iv.next = add i32 %iv, 1
  %done = icmp eq i32 %iv.next, 2
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @tgt(ptr %p, i1 %c) {
entry:
  br label %vector.ph

vector.ph:                                        ; preds = %entry
  br label %vector.body

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i32 [ 0, %vector.ph ], [ %index.next, %vector.body ]
  %0 = getelementptr half, ptr %p, i32 %index
  %wide.load = load <2 x half>, ptr %0, align 2
  %predphi = select nnan i1 %c, <2 x half> zeroinitializer, <2 x half> %wide.load
  store <2 x half> %predphi, ptr %0, align 2
  %index.next = add nuw i32 %index, 2
  %1 = icmp eq i32 %index.next, 2
  br i1 %1, label %middle.block, label %vector.body, !llvm.loop !0

middle.block:                                     ; preds = %vector.body
  br label %exit

exit:                                             ; preds = %middle.block
  ret void
}

!0 = distinct !{!0, !1, !2}
!1 = !{!"llvm.loop.isvectorized", i32 1}
!2 = !{!"llvm.loop.unroll.runtime.disable"}
