# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/rossierd/soo.tech/soo/bsp/merida/fastboot

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build

# Include any dependencies generated for this target.
include android-system-core/adb/CMakeFiles/diagnose_usb.dir/depend.make

# Include the progress variables for this target.
include android-system-core/adb/CMakeFiles/diagnose_usb.dir/progress.make

# Include the compile flags for this target's objects.
include android-system-core/adb/CMakeFiles/diagnose_usb.dir/flags.make

android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o: android-system-core/adb/CMakeFiles/diagnose_usb.dir/flags.make
android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o: ../android-system-core/adb/diagnose_usb.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/adb && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/adb/diagnose_usb.cpp

android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/adb && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/adb/diagnose_usb.cpp > CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.i

android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/adb && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/adb/diagnose_usb.cpp -o CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.s

android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o.requires:

.PHONY : android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o.requires

android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o.provides: android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o.requires
	$(MAKE) -f android-system-core/adb/CMakeFiles/diagnose_usb.dir/build.make android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o.provides.build
.PHONY : android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o.provides

android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o.provides.build: android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o


# Object files for target diagnose_usb
diagnose_usb_OBJECTS = \
"CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o"

# External object files for target diagnose_usb
diagnose_usb_EXTERNAL_OBJECTS =

android-system-core/adb/libdiagnose_usb.a: android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o
android-system-core/adb/libdiagnose_usb.a: android-system-core/adb/CMakeFiles/diagnose_usb.dir/build.make
android-system-core/adb/libdiagnose_usb.a: android-system-core/adb/CMakeFiles/diagnose_usb.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library libdiagnose_usb.a"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/adb && $(CMAKE_COMMAND) -P CMakeFiles/diagnose_usb.dir/cmake_clean_target.cmake
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/adb && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/diagnose_usb.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
android-system-core/adb/CMakeFiles/diagnose_usb.dir/build: android-system-core/adb/libdiagnose_usb.a

.PHONY : android-system-core/adb/CMakeFiles/diagnose_usb.dir/build

android-system-core/adb/CMakeFiles/diagnose_usb.dir/requires: android-system-core/adb/CMakeFiles/diagnose_usb.dir/diagnose_usb.cpp.o.requires

.PHONY : android-system-core/adb/CMakeFiles/diagnose_usb.dir/requires

android-system-core/adb/CMakeFiles/diagnose_usb.dir/clean:
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/adb && $(CMAKE_COMMAND) -P CMakeFiles/diagnose_usb.dir/cmake_clean.cmake
.PHONY : android-system-core/adb/CMakeFiles/diagnose_usb.dir/clean

android-system-core/adb/CMakeFiles/diagnose_usb.dir/depend:
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/rossierd/soo.tech/soo/bsp/merida/fastboot /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/adb /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/adb /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/adb/CMakeFiles/diagnose_usb.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : android-system-core/adb/CMakeFiles/diagnose_usb.dir/depend
