; target: 64 bits ptr addr
target datalayout = "p:64:64:64"
@x = global i32 0
@y = global i64 0

define i1 @disj() {
  ret i1 true
}

define i1 @disj_false() {
  ret i1 false
}
