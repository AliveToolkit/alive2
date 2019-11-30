; target: 64 bits ptr addr
target datalayout = "p:64:64:64"
@x = external global i32
@y = external global i64

define i1 @disj() {
  ret i1 true
}

define i1 @disj_false() {
  ret i1 false
}
