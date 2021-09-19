define void @src(i8* byval(i8) %x) readonly {
  store i8 3, i8* %x
  ret void
}

define void @tgt(i8* byval(i8) %x) readonly {  
  unreachable
}
