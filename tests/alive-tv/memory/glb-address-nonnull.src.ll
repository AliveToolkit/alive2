target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
@x = global i32 0, align 4

define i1 @f() {
  %i = ptrtoint i32* @x to i64
  %x = icmp eq i64 %i, 0
  ret i1 %x
}
