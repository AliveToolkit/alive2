; this is needed for correctness, since the cmn+branch needs to be strong
; TEST-ARGS: -disable-undef-input

define i8* @src(i8* %a, i8* nocapture %b) {
  %cmp = icmp eq i8* %a, %b
  br i1 %cmp, label %t, label %f

t:
  %v = call i8* @g(i8* %b)
  ret i8* %v

f:
  ret i8* null
}

define i8* @tgt(i8* %a, i8* nocapture %b) {
  %cmp = icmp eq i8* %a, %b
  br i1 %cmp, label %t, label %f

t:
  %v = call i8* @g(i8* %a)
  ret i8* %v

f:
  ret i8* null
}


declare i8* @g(i8*)
