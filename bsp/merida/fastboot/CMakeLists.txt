cmake_minimum_required(VERSION 3.1)

project(fastboot)
set(CMAKE_CXX_STANDARD 14)

include_directories(
	include-aosp/
	include-aosp/mkbootimg/
	android-system-core/adb/
	android-system-core/libsparse/include/
	android-system-extras/f2fs_utils/
)

# main sources
add_subdirectory(fastboot)

# needed dependancies
add_subdirectory(android-system-core)
add_subdirectory(android-system-extras)

