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
CMAKE_SOURCE_DIR = /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build

# Include any dependencies generated for this target.
include CMakeFiles/achtool.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/achtool.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/achtool.dir/flags.make

CMakeFiles/achtool.dir/src/achtool.o: CMakeFiles/achtool.dir/flags.make
CMakeFiles/achtool.dir/src/achtool.o: ../src/achtool.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/achtool.dir/src/achtool.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/achtool.dir/src/achtool.o   -c /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/achtool.c

CMakeFiles/achtool.dir/src/achtool.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/achtool.dir/src/achtool.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/achtool.c > CMakeFiles/achtool.dir/src/achtool.i

CMakeFiles/achtool.dir/src/achtool.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/achtool.dir/src/achtool.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/achtool.c -o CMakeFiles/achtool.dir/src/achtool.s

CMakeFiles/achtool.dir/src/achtool.o.requires:

.PHONY : CMakeFiles/achtool.dir/src/achtool.o.requires

CMakeFiles/achtool.dir/src/achtool.o.provides: CMakeFiles/achtool.dir/src/achtool.o.requires
	$(MAKE) -f CMakeFiles/achtool.dir/build.make CMakeFiles/achtool.dir/src/achtool.o.provides.build
.PHONY : CMakeFiles/achtool.dir/src/achtool.o.provides

CMakeFiles/achtool.dir/src/achtool.o.provides.build: CMakeFiles/achtool.dir/src/achtool.o


CMakeFiles/achtool.dir/src/achutil.o: CMakeFiles/achtool.dir/flags.make
CMakeFiles/achtool.dir/src/achutil.o: ../src/achutil.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/achtool.dir/src/achutil.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/achtool.dir/src/achutil.o   -c /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/achutil.c

CMakeFiles/achtool.dir/src/achutil.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/achtool.dir/src/achutil.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/achutil.c > CMakeFiles/achtool.dir/src/achutil.i

CMakeFiles/achtool.dir/src/achutil.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/achtool.dir/src/achutil.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/achutil.c -o CMakeFiles/achtool.dir/src/achutil.s

CMakeFiles/achtool.dir/src/achutil.o.requires:

.PHONY : CMakeFiles/achtool.dir/src/achutil.o.requires

CMakeFiles/achtool.dir/src/achutil.o.provides: CMakeFiles/achtool.dir/src/achutil.o.requires
	$(MAKE) -f CMakeFiles/achtool.dir/build.make CMakeFiles/achtool.dir/src/achutil.o.provides.build
.PHONY : CMakeFiles/achtool.dir/src/achutil.o.provides

CMakeFiles/achtool.dir/src/achutil.o.provides.build: CMakeFiles/achtool.dir/src/achutil.o


# Object files for target achtool
achtool_OBJECTS = \
"CMakeFiles/achtool.dir/src/achtool.o" \
"CMakeFiles/achtool.dir/src/achutil.o"

# External object files for target achtool
achtool_EXTERNAL_OBJECTS =

ach: CMakeFiles/achtool.dir/src/achtool.o
ach: CMakeFiles/achtool.dir/src/achutil.o
ach: CMakeFiles/achtool.dir/build.make
ach: libach.so
ach: CMakeFiles/achtool.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C executable ach"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/achtool.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/achtool.dir/build: ach

.PHONY : CMakeFiles/achtool.dir/build

CMakeFiles/achtool.dir/requires: CMakeFiles/achtool.dir/src/achtool.o.requires
CMakeFiles/achtool.dir/requires: CMakeFiles/achtool.dir/src/achutil.o.requires

.PHONY : CMakeFiles/achtool.dir/requires

CMakeFiles/achtool.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/achtool.dir/cmake_clean.cmake
.PHONY : CMakeFiles/achtool.dir/clean

CMakeFiles/achtool.dir/depend:
	cd /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build/CMakeFiles/achtool.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/achtool.dir/depend
