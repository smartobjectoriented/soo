set(libdiagnose_usb_SRCS
	diagnose_usb.cpp
)

add_definitions(-Wall -Wextra -Werror)

add_library(diagnose_usb ${libdiagnose_usb_SRCS})

