target datalayout = "e-p:64:64:64"

define i32 @lt() {
  ret i32 -11 ; any negative number is fine
}

define i32 @lt2() {
  ret i32 -99
}

define i32 @lt3() {
  ret i32 -2553
}

define i32 @lt4() {
  ret i32 -1
}

define i32 @eq() {
  ret i32 0
}

define i32 @gt() {
  ret i32 101
}

define i32 @gt2() {
  ret i32 2147483647
}
