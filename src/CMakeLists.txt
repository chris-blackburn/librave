# Some functions in libelf require libz
find_package(ZLIB REQUIRED)
find_library(ELF elf)
if(NOT ELF)
	message(FATAL_ERROR "libelf is required for this project")
endif()

# Library
add_library(rave STATIC
	rave.c
	binary.c
	section.c
	segment.c
)

target_compile_options(rave PRIVATE
	"-Wall"
	"-Werror"
)

target_link_libraries(rave PRIVATE
	-static
	${ELF}
	${ZLIB_LIBRARIES}
)

set_target_properties(rave PROPERTIES PUBLIC_HEADER
	"${PROJECT_SOURCE_DIR}/include/rave.h"
)
