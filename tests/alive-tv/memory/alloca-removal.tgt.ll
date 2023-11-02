define ptr @f(i64 %size) {
entry:
  %call = call noalias ptr @malloc(i64 %size)
  ret ptr %call
}

define ptr @f_observed(i64 %size) {
entry:
  %call = call noalias ptr @malloc(i64 %size)
  %unused = ptrtoint ptr %call to i64
  ret ptr %call
}

define ptr @f_observed2(i64 %size) {
entry:
  %call = call noalias ptr @calloc(i64 4, i64 %size)
  %unused = ptrtoint ptr %call to i64
  ret ptr %call
}

define ptr @f_observed3(i64 %size) {
entry:
  %unused0 = bitcast i64 %size to i64
  %call = call noalias ptr @calloc(i64 4, i64 %size)
  %unused = ptrtoint ptr %call to i64
  ret ptr %call
}

declare noalias ptr @malloc(i64)
declare noalias ptr @calloc(i64, i64)
