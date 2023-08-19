target datalayout = "i24:32:32" ; 4-bytes aligned
; LangRef says zeroinitializer is exactly equivalent to explicit zero initialization

define void @f(ptr %ptr) {
  store [4 x i24] [i24 0, i24 0, i24 0, i24 0], ptr %ptr
  ret void
}
