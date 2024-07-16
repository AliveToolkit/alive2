
declare ptr @objc_retainAutoreleasedReturnValue()
declare ptr @fn1()

define void @src() {
  call ptr @fn1() [ "clang.arc.attachedcall"(ptr @objc_retainAutoreleasedReturnValue) ]
  ret void
}

define void @tgt() {
  call ptr @fn1()
  call ptr @objc_retainAutoreleasedReturnValue()
  ret void
}
