cmake_minimum_required(VERSION 3.4)

project(Alive2)
set(CMAKE_BUILD_TYPE Debug)

if (MSVC)
  set(CMAKE_CXX_FLAGS                "/GL /EHsc /W2")
  set(CMAKE_CXX_FLAGS_DEBUG          "/Od /Zi")
  set(CMAKE_CXX_FLAGS_RELEASE        "/O2 /Oi /Oy /Zc:inline")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Oi /Zi")
  set(CMAKE_EXE_LINKER_FLAGS         "/LTCG:INCREMENTAL ${CMAKE_EXE_LINKER_FLAGS}")
else()
  set(CMAKE_CXX_FLAGS                "-std=c++17 -Wall -march=native")
  set(CMAKE_CXX_FLAGS_DEBUG          "-Werror -g -Og")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g")
endif()

# cmake doesn't support C++17 yet
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if (CYGWIN)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
endif()

find_package(Z3 REQUIRED)
include_directories(${Z3_INCLUDE_DIRS})

find_program(RE2C re2c)
message(STATUS "RE2C: ${RE2C}")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tools"
                   COMMAND ${RE2C} ARGS "-d" "-b" "--no-generation-date"
                   "-o" "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   "${PROJECT_SOURCE_DIR}/tools/alive_lexer.re"
                   DEPENDS "tools/alive_lexer.re")

include_directories(${PROJECT_SOURCE_DIR})

set(IR_SRC
    ir/function.cpp
    ir/instr.cpp
    ir/state.cpp
    ir/type.cpp
    ir/value.cpp
  )

set(SMT_SRC
    smt/ctx.cpp
    smt/expr.cpp
    smt/smt.cpp
    smt/solver.cpp
  )

set(UTIL_SRC
    util/compiler.cpp
    util/config.cpp
    util/errors.cpp
    util/file.cpp
    util/symexec.cpp
  )

add_executable(alive
               ${IR_SRC}
               ${SMT_SRC}
               ${UTIL_SRC}
               "tools/alive.cpp"
               "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
               "tools/alive_parser.cpp"
               "tools/transform.cpp"
              )

if (CYGWIN)
  # cmake on cygwin doesn't seem to know about dlls..
  get_filename_component(Z3_LIB_DIR "${Z3_LIBRARIES}" DIRECTORY)
  set(CMAKE_EXE_LINKER_FLAGS "-L${Z3_LIB_DIR} -lz3")
  file(COPY "${Z3_LIBRARIES}" DESTINATION "${PROJECT_BINARY_DIR}")
else()
  target_link_libraries(alive ${Z3_LIBRARIES})
endif()
