cmake_minimum_required(VERSION 3.1)

project(librave)
set(librave_VERSION_MAJOR 0)
set(librave_VERSION_MINOR 1)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "Why would you try to build in source?")
endif()

set(CMAKE_BUILD_TYPE "Debug")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Get the architecture
EXECUTE_PROCESS(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCH)

include_directories("${PROJECT_SOURCE_DIR}/include")
add_subdirectory(src)
add_subdirectory(test)
