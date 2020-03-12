# find SELlinux lib
find_package(PkgConfig)
pkg_check_modules(PC_LIBSELINUX QUIET libselinux)
set(LIBSELINUX2_DEFINITIONS ${PC_LIBSELINUX_CFLAGS_OTHER})

find_path(LIBSELINUX2_INCLUDE_DIR selinux/selinux.h
	HINTS ${PC_LIBSELINUX_INCLUDEDIR} ${PC_LIBSELINUX_INCLUDE_DIRS}
	PATH_SUFFIXES libselinux)

find_library(LIBSELINUX2_LIBRARY NAMES selinux libselinux
	HINTS ${PC_LIBSELINUX_LIBDIR} ${PC_LIBSELINUX_LIBRARY_DIRS} )


set(libext4_utils_SRCS
	make_ext4fs.c
	ext4fixup.c
	ext4_utils.c
	allocate.c
	contents.c
	extent.c
	indirect.c
	sha1.c
	wipe.c
	crc16.c
	ext4_sb.c
)

include_directories(${LIBSELINUX2_INCLUDE_DIR})

add_definitions(-fno-strict-aliasing)

add_library(ext4_utils ${libext4_utils_SRCS} )
target_link_libraries(ext4_utils sparse selinux)

