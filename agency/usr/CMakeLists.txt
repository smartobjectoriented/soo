cmake_minimum_required(VERSION 3.1)

project(agency-usr LANGUAGES C)

include_directories(
	include
)

link_directories(../../rootfs/host/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/)

set(CMAKE_C_FLAGS_DEBUG "-Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu89")
set(CMAKE_C_FLAGS_RELEASE "-Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu89")

add_subdirectory(core)
add_subdirectory(dcm)
add_subdirectory(frozen)

add_subdirectory(apps)

