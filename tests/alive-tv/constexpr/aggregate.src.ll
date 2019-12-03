@a = constant i32 0
@x = constant { i8*, i8* } { i8* bitcast (i32* @a to i8*), i8* bitcast (i32* @a to i8*) }

define i8* @f() {
  %a = load { i8*, i8* }, { i8*, i8* }* @x
  %b = extractvalue {i8*, i8*} %a, 0
  ret i8* %b
}

