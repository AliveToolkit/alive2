; TEST-ARGS: -src-unroll=2
; SKIP-IDENTITY
define void @src() {
entry:
  br label %BB

BB:
  br i1 undef, label %if.then, label %if.end

if.then:
  br label %if.end

if.end:
  br label %while.cond

while.cond:
  br i1 undef, label %while.cond, label %while.end

while.end:
  switch i32 undef, label %sw.default [
    i32 65, label %BB
    i32 3, label %return
    i32 57, label %BB
    i32 60, label %if.then
  ]

sw.default:
  unreachable

return:
  ret void
}

;    entry
;     |
;     BB
;   /    \
;  /      \
; if.then if.end
;           |
;         while.cond
;           |
;         while.end
;         /      \
;    sw.default  return

define void @tgt() {
  ret void
}
