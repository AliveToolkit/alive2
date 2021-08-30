declare void @use(i1)

define void @src(i8 %x, i8 %y, i1 %c) {
entry:
  br i1 %c, label %pre, label %exit

exit:
  %c.5 = icmp ugt i8 %y, 10
  call void @use(i1 %c.5)
  ret void

pre:
  %x.1 = icmp ule i8 %x, 10
  %y.1 = icmp ugt i8 %y, 99
  %and = and i1 %x.1, %y.1
  br i1 %and, label %loop, label %exit.1

loop:
  %t.1 = icmp ule i8 %x, 10
  call void @use(i1 %t.1)
  %f.1 = icmp ugt i8 %x, 10
  call void @use(i1 %f.1)
  %c.1 = icmp ule i8 %x, 9
  call void @use(i1 %c.1)
  %c.2 = icmp ugt i8 %x, 9
  call void @use(i1 %c.2)


  %t.2 = icmp ugt i8 %y, 99
  call void @use(i1 %t.2)
  %f.2 = icmp ule i8 %y, 99
  call void @use(i1 %f.2)

  %c.3 = icmp ugt i8 %y, 100
  call void @use(i1 %c.3)
  %c.4 = icmp ugt i8 %y, 100
  call void @use(i1 %c.4)

  br i1 true, label %exit, label %loop

exit.1:
  %c.6 = icmp ugt i8 %y, 10
  call void @use(i1 %c.6)
  ret void
}

define void @tgt(i8 %x, i8 %y, i1 %c) {
entry:
  br i1 %c, label %pre, label %exit

exit:                                             ; preds = %loop, %entry
  %c.5 = icmp ugt i8 %y, 10
  call void @use(i1 %c.5)
  ret void

pre:                                              ; preds = %entry
  %x.1 = icmp ule i8 %x, 10
  %y.1 = icmp ugt i8 %y, 99
  %and = and i1 %x.1, %y.1
  br i1 %and, label %loop, label %exit.1

loop:                                             ; preds = %loop, %pre
  %t.1 = icmp ule i8 %x, 10
  call void @use(i1 true)
  %f.1 = icmp ugt i8 %x, 10
  call void @use(i1 false)
  %c.1 = icmp ule i8 %x, 9
  call void @use(i1 %c.1)
  %c.2 = icmp ugt i8 %x, 9
  call void @use(i1 %c.2)
  %t.2 = icmp ugt i8 %y, 99
  call void @use(i1 true)
  %f.2 = icmp ule i8 %y, 99
  call void @use(i1 false)
  %c.3 = icmp ugt i8 %y, 100
  call void @use(i1 %c.3)
  %c.4 = icmp ugt i8 %y, 100
  call void @use(i1 %c.4)
  br i1 true, label %exit, label %loop

exit.1:                                           ; preds = %pre
  %c.6 = icmp ugt i8 %y, 10
  call void @use(i1 %c.6)
  ret void
}
