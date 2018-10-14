cmake_minimum_required(VERSION 3.4)

project(Alive2)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (MSVC)
  set(CMAKE_CXX_FLAGS                "/GL /EHsc /W2 ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "/Od /Zi")
  set(CMAKE_CXX_FLAGS_RELEASE        "/O2 /Oi /Oy /Zc:inline")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Oi /Zi")
  set(CMAKE_EXE_LINKER_FLAGS         "/LTCG:INCREMENTAL ${CMAKE_EXE_LINKER_FLAGS}")
else()
  set(CMAKE_CXX_FLAGS                "-std=c++17 -Wall -march=native -fPIC ${CMAKE_CXX_FLAGS}")
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
                   COMMAND ${RE2C} ARGS "-d" "-b" "-T" "--no-generation-date"
                   "-o" "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   "${PROJECT_SOURCE_DIR}/tools/alive_lexer.re"
                   DEPENDS "tools/alive_lexer.re")

include_directories(${PROJECT_SOURCE_DIR})

set(IR_SRCS
  ir/constant.cpp
  ir/function.cpp
  ir/instr.cpp
  ir/state.cpp
  ir/type.cpp
  ir/value.cpp
)

add_library(ir STATIC ${IR_SRCS})

set(SMT_SRCS
  smt/ctx.cpp
  smt/expr.cpp
  smt/smt.cpp
  smt/solver.cpp
)

add_library(smt STATIC ${SMT_SRCS})

set(TOOLS_SRCS
  tools/transform.cpp
)

add_library(tools STATIC ${TOOLS_SRCS})

set(UTIL_SRCS
  util/compiler.cpp
  util/config.cpp
  util/errors.cpp
  util/file.cpp
  util/symexec.cpp
)

add_library(util STATIC ${UTIL_SRCS})

set(ALIVE_LIBS ir smt tools util)

add_executable(alive
               "tools/alive.cpp"
               "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
               "tools/alive_parser.cpp"
              )
target_link_libraries(alive ${ALIVE_LIBS})

add_library(alive2 SHARED ${IR_SRCS} ${SMT_SRCS} ${TOOLS_SRCS} ${UTIL_SRCS})
target_link_libraries(alive2 ${ALIVE_LIBS})

if (CYGWIN)
  # cmake on cygwin doesn't seem to know about dlls..
  get_filename_component(Z3_LIB_DIR "${Z3_LIBRARIES}" DIRECTORY)
  set(CMAKE_EXE_LINKER_FLAGS "-L${Z3_LIB_DIR} -lz3")
  set(CMAKE_SHARED_LINKER_FLAGS "-L${Z3_LIB_DIR} -lz3")
  file(COPY "${Z3_LIBRARIES}" DESTINATION "${PROJECT_BINARY_DIR}")
else()
  target_link_libraries(alive ${Z3_LIBRARIES})
  target_link_libraries(alive2 ${Z3_LIBRARIES})
endif()

if (BUILD_TV)
  if (CYGWIN)
    message(FATAL_ERROR "LLVM plugins not supported on cygwin")
  endif()
  message(STATUS "Compiling translation validation plugin")
  add_subdirectory(tv)
else()
  message(STATUS "Skiping translation validation plugin")
endif()

add_custom_target("check"
                  COMMAND "python"
                          "${PROJECT_SOURCE_DIR}/tests/lit/lit.py"
                          "-s"
                          "${PROJECT_SOURCE_DIR}/tests"
                  DEPENDS "alive"
                 )
