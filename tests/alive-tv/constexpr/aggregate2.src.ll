@a = constant i8 0
@x = constant { i8*, i8* } { i8* @a, i8* @a }

define i8* @f() {
  %a = load { i8*, i8* }, { i8*, i8* }* @x
  %b = extractvalue {i8*, i8*} %a, 0
  ret i8* %b
}
