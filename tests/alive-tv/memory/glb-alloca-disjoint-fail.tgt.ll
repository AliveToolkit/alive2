; target: 64 bits ptr addr
target datalayout = "p:64:64:64"
@x = global i32 0

define i1 @disj() {
  ret i1 false
}

