cmake_minimum_required(VERSION 3.8)

project(Alive2)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if (NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (MSVC)
  set(CMAKE_CXX_FLAGS                "/GL /EHsc /W2 ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "/Od /Zi ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE        "/O2 /Oi /Oy /Zc:inline ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Oi /Zi ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_EXE_LINKER_FLAGS         "/LTCG:INCREMENTAL ${CMAKE_EXE_LINKER_FLAGS}")
else()
  set(CMAKE_CXX_FLAGS                "-Wall -march=native -fPIC ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "-Werror -g -Og ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if (CYGWIN)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
endif()

find_package(Z3 4.8.4 REQUIRED)

find_program(RE2C re2c)
message(STATUS "RE2C: ${RE2C}")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tools"
                   COMMAND ${RE2C} ARGS "-d" "-b" "-T" "--no-generation-date"
                   "-o" "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   "${PROJECT_SOURCE_DIR}/tools/alive_lexer.re"
                   DEPENDS "tools/alive_lexer.re")

# cmake <3.11 requires some source to be specified for add_library()
file(TOUCH "${PROJECT_BINARY_DIR}/dummy.h")
add_library(alive2 "${PROJECT_BINARY_DIR}/dummy.h")
# Do *NOT* clarify whether it should be STATIC or SHARED, defer to CMake's BUILD_SHARED_LIBS.
# All sub-libs, however, can either do the same or be explicitly specified as STATIC.

include_directories(${PROJECT_SOURCE_DIR})

set(IR_SRCS
  ir/constant.cpp
  ir/function.cpp
  ir/instr.cpp
  ir/memory.cpp
  ir/state.cpp
  ir/state_value.cpp
  ir/type.cpp
  ir/value.cpp
)
add_library(ir STATIC ${IR_SRCS})
target_link_libraries(alive2 PUBLIC ir)

set(SMT_SRCS
  smt/ctx.cpp
  smt/expr.cpp
  smt/smt.cpp
  smt/solver.cpp
)
add_library(smt STATIC ${SMT_SRCS})
target_link_libraries(smt PRIVATE Z3::Z3)
target_link_libraries(alive2 PUBLIC smt)

set(TOOLS_SRCS
  tools/transform.cpp
)
add_library(tools STATIC ${TOOLS_SRCS})
target_link_libraries(alive2 PUBLIC tools)

set(UTIL_SRCS
  util/compiler.cpp
  util/config.cpp
  util/errors.cpp
  util/file.cpp
  util/symexec.cpp
)
add_library(util STATIC ${UTIL_SRCS})
target_link_libraries(alive2 PUBLIC util)

add_executable(alive
               "tools/alive.cpp"
               "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
               "tools/alive_parser.cpp"
              )
target_link_libraries(alive PRIVATE alive2)

if (BUILD_TV AND CYGWIN)
  message(FATAL_ERROR "LLVM plugins not supported on cygwin")
endif()

if (BUILD_LLVM_UTILS OR BUILD_TV)
  find_package(LLVM REQUIRED CONFIG)

  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  if (NOT LLVM_ENABLE_RTTI)
    message(FATAL_ERROR "LLVM must be built with '-DLLVM_ENABLE_RTTI=ON'")
  endif()

  if (NOT LLVM_ENABLE_EH)
    message(FATAL_ERROR "LLVM must be built with '-DLLVM_ENABLE_EH=ON'")
  endif()

  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
  include(AddLLVM)

  llvm_map_components_to_libnames(llvm_libs support core irreader)

  add_library(ALIVELLVM INTERFACE IMPORTED)
  # We must use INTERFACE_COMPILE_OPTIONS, not INTERFACE_COMPILE_DEFINITIONS,
  # because ${LLVM_DEFINITIONS} already have -D prefixes.
  set_target_properties(ALIVELLVM PROPERTIES INTERFACE_COMPILE_OPTIONS "${LLVM_DEFINITIONS}")
  set_target_properties(ALIVELLVM PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LLVM_INCLUDE_DIRS}")
  set_target_properties(ALIVELLVM PROPERTIES INTERFACE_LINK_LIBRARIES "${llvm_libs}")

  set(LLVM_UTIL_SRCS
    "llvm_util/utils.cpp"
  )
  add_library(llvm_util STATIC ${LLVM_UTIL_SRCS})
  target_link_libraries(llvm_util PRIVATE ALIVELLVM)
  target_link_libraries(alive2 PUBLIC llvm_util)

  add_llvm_executable(alive-tv
    "tools/alive-tv.cpp"
  )
  target_link_libraries(alive-tv PRIVATE ALIVELLVM)
  target_link_libraries(alive-tv PRIVATE alive2)
endif()

if (BUILD_TV)
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
                  USES_TERMINAL
                 )
