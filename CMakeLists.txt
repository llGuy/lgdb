cmake_minimum_required(VERSION "3.2")

project(lgdb)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_MBCS")

file(GLOB_RECURSE LGDB_SOURCES "source/lgdb/*.c" "source/lgdb/*.h")
file(GLOB_RECURSE CLI_SOURCES "source/cli/*.cpp" "source/cli/*.hpp")
file(GLOB_RECURSE TEST_SOURCES "source/test/*.cpp" "source/test/*.hpp")

include_directories("${CMAKE_SOURCE_DIR}/source/lgdb")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/zydis/include")

add_library(lgdb STATIC "${LGDB_SOURCES}")
target_compile_definitions(lgdb PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(lgdb PUBLIC "dbghelp.lib" "${CMAKE_SOURCE_DIR}/dependencies/zydis/lib/Debug/Zydis.lib")

add_executable(lgdbcli "${CLI_SOURCES}")
target_link_libraries(lgdbcli PUBLIC "lgdb")

add_executable(lgdbtest "${TEST_SOURCES}")

INCLUDE (TestBigEndian)
TEST_BIG_ENDIAN(ENDIAN)
if (ENDIAN)
	message("Big endian")
	target_compile_definitions(lgdb PUBLIC BIG_ENDIAN_MACHINE)
else (ENDIAN)
	message("Small endian")
	target_compile_definitions(lgdb PUBLIC SMALL_ENDIAN_MACHINE)
endif (ENDIAN)

