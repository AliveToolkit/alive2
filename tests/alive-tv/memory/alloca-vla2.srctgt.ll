define void @src(i32 %parm) {
  %c = icmp eq i32 %parm, 0
  br i1 %c, label %foo, label %gah

gah:
  %x = shl nsw i32 %parm, 1
  %al = alloca i8, i32 %x
  br label %foo

foo:
  ret void
}

define void @tgt(i32 %parm) {
  %c = icmp eq i32 %parm, 0
  br i1 %c, label %foo, label %gah

gah:
  br label %foo

foo:
  ret void
}
