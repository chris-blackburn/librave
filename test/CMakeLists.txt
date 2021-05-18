add_executable(simple
	simple.c
)

target_compile_options(simple PRIVATE
	"-Wall"
	"-Werror"
)

target_link_libraries(simple PRIVATE
	rave
)
