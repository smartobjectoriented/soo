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
include android-system-core/liblog/CMakeFiles/log.dir/depend.make

# Include the progress variables for this target.
include android-system-core/liblog/CMakeFiles/log.dir/progress.make

# Include the compile flags for this target's objects.
include android-system-core/liblog/CMakeFiles/log.dir/flags.make

android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o: ../android-system-core/liblog/log_event_list.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/log.dir/log_event_list.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_event_list.c

android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/log.dir/log_event_list.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_event_list.c > CMakeFiles/log.dir/log_event_list.c.i

android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/log.dir/log_event_list.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_event_list.c -o CMakeFiles/log.dir/log_event_list.c.s

android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o.requires

android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o.provides: android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o.provides

android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o


android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o: ../android-system-core/liblog/log_event_write.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/log.dir/log_event_write.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_event_write.c

android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/log.dir/log_event_write.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_event_write.c > CMakeFiles/log.dir/log_event_write.c.i

android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/log.dir/log_event_write.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_event_write.c -o CMakeFiles/log.dir/log_event_write.c.s

android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o.requires

android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o.provides: android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o.provides

android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o


android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o: ../android-system-core/liblog/logger_write.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/log.dir/logger_write.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_write.c

android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/log.dir/logger_write.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_write.c > CMakeFiles/log.dir/logger_write.c.i

android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/log.dir/logger_write.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_write.c -o CMakeFiles/log.dir/logger_write.c.s

android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o.requires

android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o.provides: android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o.provides

android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o


android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o: ../android-system-core/liblog/config_write.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building C object android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/log.dir/config_write.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/config_write.c

android-system-core/liblog/CMakeFiles/log.dir/config_write.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/log.dir/config_write.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/config_write.c > CMakeFiles/log.dir/config_write.c.i

android-system-core/liblog/CMakeFiles/log.dir/config_write.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/log.dir/config_write.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/config_write.c -o CMakeFiles/log.dir/config_write.c.s

android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o.requires

android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o.provides: android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o.provides

android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o


android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o: ../android-system-core/liblog/logger_name.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building C object android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/log.dir/logger_name.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_name.c

android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/log.dir/logger_name.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_name.c > CMakeFiles/log.dir/logger_name.c.i

android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/log.dir/logger_name.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_name.c -o CMakeFiles/log.dir/logger_name.c.s

android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o.requires

android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o.provides: android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o.provides

android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o


android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o: ../android-system-core/liblog/logger_lock.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building C object android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/log.dir/logger_lock.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_lock.c

android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/log.dir/logger_lock.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_lock.c > CMakeFiles/log.dir/logger_lock.c.i

android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/log.dir/logger_lock.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/logger_lock.c -o CMakeFiles/log.dir/logger_lock.c.s

android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o.requires

android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o.provides: android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o.provides

android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o


android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o: ../android-system-core/liblog/log_ratelimit.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/log.dir/log_ratelimit.cpp.o -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_ratelimit.cpp

android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/log.dir/log_ratelimit.cpp.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_ratelimit.cpp > CMakeFiles/log.dir/log_ratelimit.cpp.i

android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/log.dir/log_ratelimit.cpp.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/log_ratelimit.cpp -o CMakeFiles/log.dir/log_ratelimit.cpp.s

android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o.requires

android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o.provides: android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o.provides

android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o


android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o: ../android-system-core/liblog/fake_log_device.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building C object android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/log.dir/fake_log_device.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/fake_log_device.c

android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/log.dir/fake_log_device.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/fake_log_device.c > CMakeFiles/log.dir/fake_log_device.c.i

android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/log.dir/fake_log_device.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/fake_log_device.c -o CMakeFiles/log.dir/fake_log_device.c.s

android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o.requires

android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o.provides: android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o.provides

android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o


android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o: android-system-core/liblog/CMakeFiles/log.dir/flags.make
android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o: ../android-system-core/liblog/fake_writer.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Building C object android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/log.dir/fake_writer.c.o   -c /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/fake_writer.c

android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/log.dir/fake_writer.c.i"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/fake_writer.c > CMakeFiles/log.dir/fake_writer.c.i

android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/log.dir/fake_writer.c.s"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog/fake_writer.c -o CMakeFiles/log.dir/fake_writer.c.s

android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o.requires:

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o.requires

android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o.provides: android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o.requires
	$(MAKE) -f android-system-core/liblog/CMakeFiles/log.dir/build.make android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o.provides.build
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o.provides

android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o.provides.build: android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o


# Object files for target log
log_OBJECTS = \
"CMakeFiles/log.dir/log_event_list.c.o" \
"CMakeFiles/log.dir/log_event_write.c.o" \
"CMakeFiles/log.dir/logger_write.c.o" \
"CMakeFiles/log.dir/config_write.c.o" \
"CMakeFiles/log.dir/logger_name.c.o" \
"CMakeFiles/log.dir/logger_lock.c.o" \
"CMakeFiles/log.dir/log_ratelimit.cpp.o" \
"CMakeFiles/log.dir/fake_log_device.c.o" \
"CMakeFiles/log.dir/fake_writer.c.o"

# External object files for target log
log_EXTERNAL_OBJECTS =

android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/build.make
android-system-core/liblog/liblog.a: android-system-core/liblog/CMakeFiles/log.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_10) "Linking CXX static library liblog.a"
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && $(CMAKE_COMMAND) -P CMakeFiles/log.dir/cmake_clean_target.cmake
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/log.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
android-system-core/liblog/CMakeFiles/log.dir/build: android-system-core/liblog/liblog.a

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/build

android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/log_event_list.c.o.requires
android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/log_event_write.c.o.requires
android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/logger_write.c.o.requires
android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/config_write.c.o.requires
android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/logger_name.c.o.requires
android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/logger_lock.c.o.requires
android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/log_ratelimit.cpp.o.requires
android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/fake_log_device.c.o.requires
android-system-core/liblog/CMakeFiles/log.dir/requires: android-system-core/liblog/CMakeFiles/log.dir/fake_writer.c.o.requires

.PHONY : android-system-core/liblog/CMakeFiles/log.dir/requires

android-system-core/liblog/CMakeFiles/log.dir/clean:
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog && $(CMAKE_COMMAND) -P CMakeFiles/log.dir/cmake_clean.cmake
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/clean

android-system-core/liblog/CMakeFiles/log.dir/depend:
	cd /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/rossierd/soo.tech/soo/bsp/merida/fastboot /home/rossierd/soo.tech/soo/bsp/merida/fastboot/android-system-core/liblog /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog /home/rossierd/soo.tech/soo/bsp/merida/fastboot/build/android-system-core/liblog/CMakeFiles/log.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : android-system-core/liblog/CMakeFiles/log.dir/depend
