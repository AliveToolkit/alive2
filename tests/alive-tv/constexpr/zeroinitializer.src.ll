target datalayout = "i24:32:32" ; 4-bytes aligned
; LangRef says zeroinitializer is exactly equivalent to explicit zero initialization

define void @f([4 x i24]* %ptr) {
  store [4 x i24] zeroinitializer, [4 x i24]* %ptr
  ret void
}
