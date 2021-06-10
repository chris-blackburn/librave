# TODO: Need to make this better if I intend to have more tests...

add_executable(simple
	simple.c
)

target_compile_options(simple PRIVATE
	"-Wall"
	"-Werror"
	-static
)

target_link_libraries(simple PRIVATE
	rave
)

add_executable(code_mapping
	code_mapping.c
)

target_compile_options(code_mapping PRIVATE
	"-Wall"
	"-Werror"
	-static
)

target_link_libraries(code_mapping PRIVATE
	rave
)
