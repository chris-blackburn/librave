# Some functions in libelf require libz
find_package(ZLIB REQUIRED)
find_library(ELF elf)
if(NOT ELF)
	message(FATAL_ERROR "libelf is required for this project")
endif()
find_library(DWARF dwarf)
if(NOT DWARF)
	message(FATAL_ERROR "libdwarf is required for this project")
endif()

# Find DynamoRIO
set(DYNAMORIO_INSTALL "${PROJECT_SOURCE_DIR}/deps/DynamoRIO")
find_path(DYNAMORIO_LIB_DIR
	NAMES libdrdecode.a
	PATHS ${DYNAMORIO_INSTALL}/lib64/release
)
find_path(DYNAMORIO_INC_DIR
	NAMES dr_api.h
	PATHS ${DYNAMORIO_INSTALL}/include
)

# Library
add_library(rave STATIC
	rave.c
	binary.c
	section.c
	segment.c
	metadata_dwarf.c
	transform.c
	window.c
	random.c
)

target_include_directories(rave PRIVATE
	"${PROJECT_SOURCE_DIR}/include"
	"${DYNAMORIO_INC_DIR}"
)

target_compile_options(rave PRIVATE
	"-Wall"
	"-Wextra"
	"-Werror"
)

target_link_libraries(rave PRIVATE
	${DWARF}
	${ELF}
	${ZLIB_LIBRARIES}
	${DYNAMORIO_LIB_DIR}/libdrdecode.a
	${DYNAMORIO_LIB_DIR}/../libdrlibc.a
	-static
)

set_target_properties(rave PROPERTIES PUBLIC_HEADER
	"${PROJECT_SOURCE_DIR}/include/rave.h"
)
