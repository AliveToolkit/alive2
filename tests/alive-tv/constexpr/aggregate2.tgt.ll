@a = constant i8 0
@x = constant { i8*, i8* } { i8* @a, i8* @a }

define i8* @f() {
  ret i8* @a
}
