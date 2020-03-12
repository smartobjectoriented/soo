set(libziparchive_SRCS
	zip_archive.cc
	zip_archive_stream_entry.cc
	zip_writer.cc
)

add_definitions(-Werror -Wall -Wold-style-cast -Wno-missing-field-initializers)

add_library(ziparchive ${libziparchive_SRCS} )
target_compile_definitions(ziparchive PUBLIC -DZLIB_CONST -D_FILE_OFFSET_BITS=64)
target_link_libraries(ziparchive z log utils)

