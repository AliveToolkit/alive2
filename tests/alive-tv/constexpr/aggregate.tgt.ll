@a = constant i32 0
@x = constant { i8*, i8* } { i8* bitcast (i32* @a to i8*), i8* bitcast (i32* @a to i8*) }

define i8* @f() {
  ret i8* bitcast (i32* @a to i8*)
}

