find_path(HIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h)
find_library(HIREDIS_LIBRARIES NAMES hiredis)

if(NOT HIREDIS_INCLUDE_DIR)
  message(FATAL_ERROR "hiredis include directory not found")
endif()

if(NOT HIREDIS_LIBRARIES)
  message(FATAL_ERROR "hiredis library not found")
endif()

message(STATUS "hiredis: ${HIREDIS_INCLUDE_DIR} ${HIREDIS_LIBRARIES}")
