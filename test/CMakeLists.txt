add_compile_options(
	"-Wall"
	"-Werror"
)
link_libraries(rave)

add_executable(dump_text dump_text.c)
add_executable(rewrite rewrite.c)
add_executable(code_mapping code_mapping.c)
