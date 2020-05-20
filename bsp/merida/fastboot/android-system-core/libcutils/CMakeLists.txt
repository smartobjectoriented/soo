set(libcutils_SRCS
	sockets.cpp
	sockets_unix.cpp
	socket_network_client_unix.c
	socket_inaddr_any_server_unix.c
	android_get_control_file.cpp
)

add_definitions(-Wall -Wextra -Werror)

add_library(cutils ${libcutils_SRCS} )
