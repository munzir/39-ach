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
include CMakeFiles/achtest.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/achtest.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/achtest.dir/flags.make

CMakeFiles/achtest.dir/src/test/achtest.o: CMakeFiles/achtest.dir/flags.make
CMakeFiles/achtest.dir/src/test/achtest.o: ../src/test/achtest.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/achtest.dir/src/test/achtest.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/achtest.dir/src/test/achtest.o   -c /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/test/achtest.c

CMakeFiles/achtest.dir/src/test/achtest.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/achtest.dir/src/test/achtest.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/test/achtest.c > CMakeFiles/achtest.dir/src/test/achtest.i

CMakeFiles/achtest.dir/src/test/achtest.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/achtest.dir/src/test/achtest.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/src/test/achtest.c -o CMakeFiles/achtest.dir/src/test/achtest.s

CMakeFiles/achtest.dir/src/test/achtest.o.requires:

.PHONY : CMakeFiles/achtest.dir/src/test/achtest.o.requires

CMakeFiles/achtest.dir/src/test/achtest.o.provides: CMakeFiles/achtest.dir/src/test/achtest.o.requires
	$(MAKE) -f CMakeFiles/achtest.dir/build.make CMakeFiles/achtest.dir/src/test/achtest.o.provides.build
.PHONY : CMakeFiles/achtest.dir/src/test/achtest.o.provides

CMakeFiles/achtest.dir/src/test/achtest.o.provides.build: CMakeFiles/achtest.dir/src/test/achtest.o


# Object files for target achtest
achtest_OBJECTS = \
"CMakeFiles/achtest.dir/src/test/achtest.o"

# External object files for target achtest
achtest_EXTERNAL_OBJECTS =

achtest: CMakeFiles/achtest.dir/src/test/achtest.o
achtest: CMakeFiles/achtest.dir/build.make
achtest: libach.so
achtest: CMakeFiles/achtest.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable achtest"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/achtest.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/achtest.dir/build: achtest

.PHONY : CMakeFiles/achtest.dir/build

CMakeFiles/achtest.dir/requires: CMakeFiles/achtest.dir/src/test/achtest.o.requires

.PHONY : CMakeFiles/achtest.dir/requires

CMakeFiles/achtest.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/achtest.dir/cmake_clean.cmake
.PHONY : CMakeFiles/achtest.dir/clean

CMakeFiles/achtest.dir/depend:
	cd /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build /home/munzir/Me/5-Work/01-PhD/01-WholeBodyControlAttempt1/39-ach/build/CMakeFiles/achtest.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/achtest.dir/depend

