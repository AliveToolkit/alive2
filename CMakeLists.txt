cmake_minimum_required(VERSION 3.8)
include(ProcessorCount)

project(Alive2)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

if (MSVC)
  set(CMAKE_CXX_FLAGS                "/GL /EHsc /W2 ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "/Od /Zi ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE        "/O2 /Oi /Oy /Zc:inline ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Oi /Zi ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_EXE_LINKER_FLAGS         "/LTCG:INCREMENTAL ${CMAKE_EXE_LINKER_FLAGS}")
else()
  set(CMAKE_CXX_FLAGS                "-Wall -Werror -march=native -fPIC ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG          "-g -Og ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

macro(remove_flags)
  foreach(var ${ARGN})
    string(REPLACE "${var}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "${var}" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "${var}" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "${var}" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    remove_definitions(${var})
  endforeach()
endmacro(remove_flags)

remove_flags(-DNDEBUG)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if (CYGWIN)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
endif()

find_package(Git REQUIRED)
add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/version_gen.h.tmp"
  COMMAND "${CMAKE_COMMAND}" -E echo_append "#define ALIVE_VERSION_MACRO " > "${CMAKE_BINARY_DIR}/version_gen.h.tmp"
  COMMAND "${GIT_EXECUTABLE}" describe --tags --dirty --always >> "${CMAKE_BINARY_DIR}/version_gen.h.tmp"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  VERBATIM
)
add_custom_command(
  DEPENDS "${CMAKE_BINARY_DIR}/version_gen.h.tmp"
  OUTPUT "${CMAKE_BINARY_DIR}/version_gen.h"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_BINARY_DIR}/version_gen.h.tmp" "${CMAKE_BINARY_DIR}/version_gen.h"
  COMMAND "${CMAKE_COMMAND}" -E remove -f "${CMAKE_BINARY_DIR}/version_gen.h.tmp"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  VERBATIM
)
add_custom_target(
  generate_version ALL
  DEPENDS "${CMAKE_BINARY_DIR}/version_gen.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

find_package(ZLIB)
find_package(Z3 4.8.5 REQUIRED)
include_directories(${Z3_INCLUDE_DIR})

find_program(RE2C re2c)
if (RE2C)
  message(STATUS "RE2C: ${RE2C}")
else()
  message(SEND_ERROR "re2c executable not found")
endif()
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tools"
                   COMMAND ${RE2C} ARGS "-d" "-b" "-T" "--no-generation-date"
                   "-o" "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
                   "${PROJECT_SOURCE_DIR}/tools/alive_lexer.re"
                   DEPENDS "tools/alive_lexer.re")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_BINARY_DIR})

set(IR_SRCS
  ir/attrs.cpp
  ir/constant.cpp
  ir/function.cpp
  ir/globals.cpp
  ir/instr.cpp
  ir/memory.cpp
  ir/precondition.cpp
  ir/state.cpp
  ir/state_value.cpp
  ir/type.cpp
  ir/value.cpp
)

add_library(ir STATIC ${IR_SRCS})

set(SMT_SRCS
  smt/ctx.cpp
  smt/expr.cpp
  smt/exprs.cpp
  smt/smt.cpp
  smt/solver.cpp
)

add_library(smt STATIC ${SMT_SRCS})

set(TOOLS_SRCS
  tools/transform.cpp
)

add_library(tools STATIC ${TOOLS_SRCS})

set(UTIL_SRCS
  "${CMAKE_BINARY_DIR}/version_gen.h"
  util/version.cpp
  util/compiler.cpp
  util/config.cpp
  util/errors.cpp
  util/file.cpp
  util/parallel_jobserver.cpp
  util/parallel_unrestricted.cpp
  util/sort.cpp
  util/stopwatch.cpp
  util/symexec.cpp
  util/unionfind.cpp
)

add_library(util STATIC ${UTIL_SRCS})
add_dependencies(util generate_version)

set(ALIVE_LIBS ir smt tools util)


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

  include_directories(${LLVM_INCLUDE_DIRS})
  add_definitions(${LLVM_DEFINITIONS})

  set(LLVM_UTIL_SRCS
    "llvm_util/known_fns.cpp"
    "llvm_util/llvm2alive.cpp"
    "llvm_util/utils.cpp"
  )

  add_library(llvm_util STATIC ${LLVM_UTIL_SRCS})
  set(ALIVE_LIBS_LLVM llvm_util ${ALIVE_LIBS})

  add_llvm_executable(alive-tv
    "tools/alive-tv.cpp"
  )

  add_llvm_executable(alive-exec
    "tools/alive-exec.cpp"
  )

else()
  set(LLVM_UTIL_SRCS "")
endif()

if (BUILD_TV)
  if (CYGWIN)
    message(FATAL_ERROR "LLVM plugins not supported on cygwin")
  endif()
  message(STATUS "Compiling translation validation plugin")
  add_subdirectory(tv)
  configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/opt-alive.sh.in
    ${CMAKE_BINARY_DIR}/opt-alive.sh
    @ONLY
  )
else()
  message(STATUS "Skipping translation validation plugin")
endif()

add_executable(alive
               "tools/alive.cpp"
               "${CMAKE_BINARY_DIR}/tools/alive_lexer.cpp"
               "tools/alive_parser.cpp"
              )
target_link_libraries(alive PRIVATE ${ALIVE_LIBS})

#add_library(alive2 SHARED ${IR_SRCS} ${SMT_SRCS} ${TOOLS_SRCS} ${UTIL_SRCS} ${LLVM_UTIL_SRCS})

if (BUILD_LLVM_UTILS OR BUILD_TV)
  llvm_map_components_to_libnames(llvm_libs support core irreader analysis passes transformutils)
#  target_link_libraries(alive2 PRIVATE ${llvm_libs})
  target_link_libraries(alive-tv PRIVATE ${ALIVE_LIBS_LLVM} ${llvm_libs})
  target_link_libraries(alive-exec PRIVATE ${ALIVE_LIBS_LLVM} ${llvm_libs})
endif()

if (CYGWIN)
  # cmake on cygwin doesn't seem to know about dlls..
  get_filename_component(Z3_LIB_DIR "${Z3_LIBRARIES}" DIRECTORY)
  set(CMAKE_EXE_LINKER_FLAGS "-L${Z3_LIB_DIR} -lz3")
  set(CMAKE_SHARED_LINKER_FLAGS "-L${Z3_LIB_DIR} -lz3")
  file(COPY "${Z3_LIBRARIES}" DESTINATION "${PROJECT_BINARY_DIR}")
else()
  target_link_libraries(alive PRIVATE ${Z3_LIBRARIES})
  #target_link_libraries(alive2 PRIVATE ${Z3_LIBRARIES})
  if (BUILD_LLVM_UTILS OR BUILD_TV)
    target_link_libraries(alive-tv PRIVATE ${Z3_LIBRARIES})
    target_link_libraries(alive-exec PRIVATE ${Z3_LIBRARIES})
  endif()
endif()

if (NOT DEFINED TEST_NTHREADS)
  ProcessorCount(TEST_NTHREADS)
  if (TEST_NTHREADS EQUAL 0)
    set(TEST_NTHREADS 1)
  endif()
endif()
add_custom_target("check"
                  COMMAND "python"
                          "${PROJECT_SOURCE_DIR}/tests/lit/lit.py"
                          "-s"
                          "${PROJECT_SOURCE_DIR}/tests"
                          "-j${TEST_NTHREADS}"
                  DEPENDS "alive"
                  USES_TERMINAL
                 )

if (BUILD_TV)
  add_dependencies("check" "alive-tv" "alive-exec")
endif()
