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
include android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/depend.make

# Include the progress variables for this target.
include android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/progress.make

# Include the compile flags for this target's objects.
include android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o: ../android-system-extras/ext4_utils/make_ext4fs.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/make_ext4fs.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/make_ext4fs.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/make_ext4fs.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/make_ext4fs.c > CMakeFiles/ext4_utils.dir/make_ext4fs.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/make_ext4fs.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/make_ext4fs.c -o CMakeFiles/ext4_utils.dir/make_ext4fs.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o: ../android-system-extras/ext4_utils/ext4fixup.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/ext4fixup.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4fixup.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/ext4fixup.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4fixup.c > CMakeFiles/ext4_utils.dir/ext4fixup.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/ext4fixup.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4fixup.c -o CMakeFiles/ext4_utils.dir/ext4fixup.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o: ../android-system-extras/ext4_utils/ext4_utils.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/ext4_utils.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4_utils.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/ext4_utils.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4_utils.c > CMakeFiles/ext4_utils.dir/ext4_utils.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/ext4_utils.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4_utils.c -o CMakeFiles/ext4_utils.dir/ext4_utils.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o: ../android-system-extras/ext4_utils/allocate.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/allocate.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/allocate.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/allocate.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/allocate.c > CMakeFiles/ext4_utils.dir/allocate.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/allocate.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/allocate.c -o CMakeFiles/ext4_utils.dir/allocate.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o: ../android-system-extras/ext4_utils/contents.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/contents.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/contents.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/contents.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/contents.c > CMakeFiles/ext4_utils.dir/contents.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/contents.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/contents.c -o CMakeFiles/ext4_utils.dir/contents.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o: ../android-system-extras/ext4_utils/extent.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/extent.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/extent.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/extent.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/extent.c > CMakeFiles/ext4_utils.dir/extent.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/extent.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/extent.c -o CMakeFiles/ext4_utils.dir/extent.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o: ../android-system-extras/ext4_utils/indirect.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/indirect.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/indirect.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/indirect.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/indirect.c > CMakeFiles/ext4_utils.dir/indirect.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/indirect.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/indirect.c -o CMakeFiles/ext4_utils.dir/indirect.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o: ../android-system-extras/ext4_utils/sha1.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/sha1.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/sha1.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/sha1.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/sha1.c > CMakeFiles/ext4_utils.dir/sha1.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/sha1.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/sha1.c -o CMakeFiles/ext4_utils.dir/sha1.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o: ../android-system-extras/ext4_utils/wipe.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/wipe.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/wipe.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/wipe.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/wipe.c > CMakeFiles/ext4_utils.dir/wipe.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/wipe.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/wipe.c -o CMakeFiles/ext4_utils.dir/wipe.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o: ../android-system-extras/ext4_utils/crc16.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_10) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/crc16.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/crc16.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/crc16.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/crc16.c > CMakeFiles/ext4_utils.dir/crc16.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/crc16.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/crc16.c -o CMakeFiles/ext4_utils.dir/crc16.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o


android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/flags.make
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o: ../android-system-extras/ext4_utils/ext4_sb.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_11) "Building C object android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ext4_utils.dir/ext4_sb.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4_sb.c

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ext4_utils.dir/ext4_sb.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4_sb.c > CMakeFiles/ext4_utils.dir/ext4_sb.c.i

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ext4_utils.dir/ext4_sb.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils/ext4_sb.c -o CMakeFiles/ext4_utils.dir/ext4_sb.c.s

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o.requires:

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o.requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o.provides: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o.requires
	$(MAKE) -f android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o.provides.build
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o.provides

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o.provides.build: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o


# Object files for target ext4_utils
ext4_utils_OBJECTS = \
"CMakeFiles/ext4_utils.dir/make_ext4fs.c.o" \
"CMakeFiles/ext4_utils.dir/ext4fixup.c.o" \
"CMakeFiles/ext4_utils.dir/ext4_utils.c.o" \
"CMakeFiles/ext4_utils.dir/allocate.c.o" \
"CMakeFiles/ext4_utils.dir/contents.c.o" \
"CMakeFiles/ext4_utils.dir/extent.c.o" \
"CMakeFiles/ext4_utils.dir/indirect.c.o" \
"CMakeFiles/ext4_utils.dir/sha1.c.o" \
"CMakeFiles/ext4_utils.dir/wipe.c.o" \
"CMakeFiles/ext4_utils.dir/crc16.c.o" \
"CMakeFiles/ext4_utils.dir/ext4_sb.c.o"

# External object files for target ext4_utils
ext4_utils_EXTERNAL_OBJECTS =

android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build.make
android-system-extras/ext4_utils/libext4_utils.a: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_12) "Linking C static library libext4_utils.a"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && $(CMAKE_COMMAND) -P CMakeFiles/ext4_utils.dir/cmake_clean_target.cmake
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ext4_utils.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build: android-system-extras/ext4_utils/libext4_utils.a

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/build

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/make_ext4fs.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4fixup.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_utils.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/allocate.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/contents.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/extent.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/indirect.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/sha1.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/wipe.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/crc16.c.o.requires
android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires: android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/ext4_sb.c.o.requires

.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/requires

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/clean:
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils && $(CMAKE_COMMAND) -P CMakeFiles/ext4_utils.dir/cmake_clean.cmake
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/clean

android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/depend:
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/rossierd/soo.tech/soo/bsp/merida/fastboot /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-extras/ext4_utils /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : android-system-extras/ext4_utils/CMakeFiles/ext4_utils.dir/depend
