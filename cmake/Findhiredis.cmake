find_path(HIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h)
find_library(HIREDIS_LIBRARIES NAMES hiredis)

message(STATUS "hiredis: ${HIREDIS_INCLUDE_DIR} ${HIREDIS_LIBRARIES}")
