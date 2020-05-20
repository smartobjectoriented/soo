set(fastboot_SRCS
	bootimg_utils.cpp
	engine.cpp
	fastboot.cpp
	fs.cpp
	protocol.cpp
	socket.cpp
	tcp.cpp
	udp.cpp
	util.cpp
)

# TODO: currently we only support building on Linux
set(fastboot_SRCS ${fastboot_SRCS}
	usb_linux.cpp
)

set(EXTRA_LIBS
	base
	diagnose_usb
	ext4_utils
	cutils
	sparse
	ziparchive
)

add_definitions(-Wall -Wextra -Werror -Wunreachable-code)

add_executable(fastboot ${fastboot_SRCS} )
target_compile_definitions(fastboot PUBLIC -DFASTBOOT_REVISION="5e9863bf63de-android")
target_link_libraries(fastboot ${EXTRA_LIBS} )

