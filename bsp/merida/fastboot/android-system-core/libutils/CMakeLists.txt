set(libutils_SRCS
	FileMap.cpp
)

add_definitions(-Wall -Wextra -Werror)

add_library(utils ${libutils_SRCS} )

