cmake_minimum_required(VERSION "3.2")

project(lgdb)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_MBCS")

file(GLOB_RECURSE LGDB_SOURCES "source/lgdb/*.c" "source/lgdb/*.h")
file(GLOB_RECURSE CLI_SOURCES "source/cli/*.cpp" "source/cli/*.hpp")
file(GLOB_RECURSE GUI_SOURCES 
	"source/gui/*.cpp" 
	"source/gui/*.hpp" 
	"dependencies/stb/*.h"
	"dependencies/imgui/lib/*.cpp" 
	"dependencies/imgui/include/*.h" 
	"dependencies/editor/*.cpp" 
	"dependencies/editor/*.h")
file(GLOB_RECURSE TEST_SOURCES "source/test/*.cpp" "source/test/*.hpp")

include_directories("${CMAKE_SOURCE_DIR}/source/lgdb")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/zydis/include")

add_library(lgdb STATIC "${LGDB_SOURCES}")
target_compile_definitions(lgdb PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(lgdb PUBLIC "dbghelp.lib" "${CMAKE_SOURCE_DIR}/dependencies/zydis/lib/Debug/Zydis.lib")

add_executable(lgdbcli "${CLI_SOURCES}")
target_link_libraries(lgdbcli PUBLIC "lgdb")

add_executable(lgdbgui "${GUI_SOURCES}")
target_link_libraries(lgdbgui PUBLIC "lgdb" 
	"${CMAKE_SOURCE_DIR}/dependencies/SDL/lib/x64/SDL2.lib" 
	"${CMAKE_SOURCE_DIR}/dependencies/glew-2.1.0/lib/Release/x64/glew32s.lib" 
	"opengl32.lib" 
	"user32.lib" 
	"gdi32.lib" 
	"shcore.lib"
	"winmm.lib")
target_include_directories(lgdbgui PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/SDL/include/")
target_include_directories(lgdbgui PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/glew-2.1.0/include")
target_include_directories(lgdbgui PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/glm/")
target_include_directories(lgdbgui PUBLIC "dependencies/imgui/include/")
target_include_directories(lgdbgui PUBLIC "dependencies/stb/")
target_include_directories(lgdbgui PUBLIC "dependencies/editor/")
target_compile_definitions(lgdbgui PUBLIC GLEW_STATIC)

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

