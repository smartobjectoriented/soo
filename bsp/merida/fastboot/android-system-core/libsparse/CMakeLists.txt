set(libsparse_SRCS
	backed_block.c
	output_file.c
	sparse.c
	sparse_crc32.c
	sparse_err.c
	sparse_read.c
)

add_definitions(-Werror)

add_library(sparse ${libsparse_SRCS} )
target_link_libraries(sparse z)

