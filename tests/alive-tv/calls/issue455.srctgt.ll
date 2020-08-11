define i32 @src(i1 %cond) {
entry:
  br label %do.body

do.body:
  br i1 %cond, label %do.end, label %if.then5

if.then5:
  unreachable

do.end:
  %call14 = call i32* @readnone()
  store i32 0, i32* %call14, align 4
  tail call void @read_error()
  unreachable
}

define i32 @tgt(i1 %cond) {
entry:
  br label %do.body

do.body:
  br i1 %cond, label %do.end, label %if.then5

if.then5:
  unreachable

do.end:
  %call14 = call i32* @readnone()
  store i32 0, i32* %call14, align 4
  tail call void @read_error()
  unreachable
}

declare i32 @f()
declare void @read_error() noreturn
declare i32* @readnone() readnone
