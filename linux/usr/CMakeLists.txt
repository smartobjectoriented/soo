
# CMake for building all agency usr applications
# It includes the basic utility like agency and other apps

cmake_minimum_required(VERSION 3.10)

project(agency-usr LANGUAGES C CXX)

option(WITH_LED_ACTIVITIES "LED activities during migration" OFF)

if (WITH_LED_ACTIVITIES)
	add_definitions(-DWITH_LED_ACTIVITIES)
endif()

include_directories(
	include
)

link_directories(../../rootfs/host/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/)

set(CMAKE_C_FLAGS_RELEASE "-Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu89")

add_subdirectory(core)
add_subdirectory(dcm)
add_subdirectory(frozen)
# EMISO - no MKTT
#add_subdirectory(mqtt_iuoc)
add_subdirectory(apps)
add_subdirectory(lib)

add_subdirectory(wago_client)

# EMISO
add_subdirectory(emiso_engine)
