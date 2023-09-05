@a = constant i8 0
@x = constant { ptr, ptr } { ptr @a, ptr @a }

define ptr @f() {
  %a = load { ptr, ptr }, ptr @x
  %b = extractvalue {ptr, ptr} %a, 0
  ret ptr %b
}
