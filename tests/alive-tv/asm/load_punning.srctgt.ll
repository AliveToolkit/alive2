; TEST-ARGS: -tgt-is-asm

@gvar1 = external global i32, align 4
@gvar2 = external global i32, align 4

define ptr @src(i32 %0) {
  switch i32 %0, label %3 [
    i32 0, label %2
  ]

2:
  br label %exit

3:
  br label %exit

exit:
  %r = phi ptr [ @gvar2, %3 ], [ @gvar1, %2 ]
  ret ptr %r
}

@.Lswitch.table.f = constant ptr @gvar1, align 8

define ptr @tgt(i32 %0) {
entry:
  %a4_7 = icmp eq i32 %0, 0
  br i1 %a4_7, label %load, label %exit

load:
  %a7_7 = load i64, ptr @.Lswitch.table.f, align 8
  %p = inttoptr i64 %a7_7 to ptr
  br label %exit

exit:
  %common.ret.op = phi ptr [ %p, %load ], [ @gvar2, %entry ]
  ret ptr %common.ret.op
}
