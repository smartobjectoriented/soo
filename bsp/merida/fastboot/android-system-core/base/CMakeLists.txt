set(libbase_SRCS
	file.cpp
	logging.cpp
	parsenetaddress.cpp
	quick_exit.cpp
	stringprintf.cpp
	strings.cpp
	test_utils.cpp
	errors_unix.cpp
)

add_definitions(-Wall -Wextra -Werror)

add_library(base ${libbase_SRCS} )
