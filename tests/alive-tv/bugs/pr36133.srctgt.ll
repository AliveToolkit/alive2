; https://bugs.llvm.org/show_bug.cgi?id=36133
@global = external local_unnamed_addr global i8*, align 8

define i32 @src(i32 %arg) local_unnamed_addr #0 {
bb:
  %tmp = load i8*, i8** @global, align 8
  %tmp1 = icmp eq i8* %tmp, null
  br i1 %tmp1, label %bb3, label %bb2

bb2:
  br label %bb3

bb3:
  %tmp4 = phi i8 [ 1, %bb2 ], [ 0, %bb ]
  %tmp5 = icmp eq i8 %tmp4, 0
  br i1 %tmp5, label %bb7, label %bb6

bb6:
  br label %bb7

bb7:
  %tmp8 = icmp eq i32 %arg, -1
  br i1 %tmp8, label %bb9, label %bb10   ; Does not depend on the content of @global, only %arg

bb9:                         ; Single pred - %bb7
  ret i32 0

bb10:                        ; Single pred - %bb7
  %tmp11 = icmp sgt i32 %arg, -1
  call void @llvm.assume(i1 %tmp11)
  ret i32 1
}

; Function Attrs: nounwind
declare void @llvm.assume(i1) #1

attributes #0 = { uwtable }
attributes #1 = { nounwind }

; Function Attrs: uwtable
define i32 @tgt(i32 %arg) local_unnamed_addr #0 {
bb:
  %tmp = load i8*, i8** @global, align 8
  %tmp1 = icmp eq i8* %tmp, null
  br i1 %tmp1, label %bb10, label %bb7             ; This now bypasses %arg check directly going to one of the possible rets

bb7:
  %tmp8 = icmp eq i32 %arg, -1
  br i1 %tmp8, label %bb9, label %bb10

bb9:
  ret i32 0

bb10:
  %tmp11 = icmp sgt i32 %arg, -1
  call void @llvm.assume(i1 %tmp11)
  ret i32 1
}

; ERROR: Source is more defined than target
