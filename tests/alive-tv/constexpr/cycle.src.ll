@g1 = constant i8* bitcast (i8** getelementptr inbounds (i8*, i8** @g2, i64 1) to i8*)
@g2 = constant i8* bitcast (i8** getelementptr inbounds (i8*, i8** @g1, i64 1) to i8*)

define i8* @f() {
  %x  = load i8*, i8** @g2
  ret i8* %x
}
