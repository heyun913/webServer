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


# Produce verbose output by default.
VERBOSE = 1

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
CMAKE_SOURCE_DIR = /root/Web-learning/sylar

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /root/Web-learning/sylar

# Include any dependencies generated for this target.
include CMakeFiles/test_config.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/test_config.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/test_config.dir/flags.make

CMakeFiles/test_config.dir/tests/test_config.cc.o: CMakeFiles/test_config.dir/flags.make
CMakeFiles/test_config.dir/tests/test_config.cc.o: tests/test_config.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/Web-learning/sylar/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/test_config.dir/tests/test_config.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) -D__FILE__=\"tests/test_config.cc\" $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/test_config.dir/tests/test_config.cc.o -c /root/Web-learning/sylar/tests/test_config.cc

CMakeFiles/test_config.dir/tests/test_config.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/test_config.dir/tests/test_config.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) -D__FILE__=\"tests/test_config.cc\" $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/Web-learning/sylar/tests/test_config.cc > CMakeFiles/test_config.dir/tests/test_config.cc.i

CMakeFiles/test_config.dir/tests/test_config.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/test_config.dir/tests/test_config.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) -D__FILE__=\"tests/test_config.cc\" $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/Web-learning/sylar/tests/test_config.cc -o CMakeFiles/test_config.dir/tests/test_config.cc.s

CMakeFiles/test_config.dir/tests/test_config.cc.o.requires:

.PHONY : CMakeFiles/test_config.dir/tests/test_config.cc.o.requires

CMakeFiles/test_config.dir/tests/test_config.cc.o.provides: CMakeFiles/test_config.dir/tests/test_config.cc.o.requires
	$(MAKE) -f CMakeFiles/test_config.dir/build.make CMakeFiles/test_config.dir/tests/test_config.cc.o.provides.build
.PHONY : CMakeFiles/test_config.dir/tests/test_config.cc.o.provides

CMakeFiles/test_config.dir/tests/test_config.cc.o.provides.build: CMakeFiles/test_config.dir/tests/test_config.cc.o


# Object files for target test_config
test_config_OBJECTS = \
"CMakeFiles/test_config.dir/tests/test_config.cc.o"

# External object files for target test_config
test_config_EXTERNAL_OBJECTS =

bin/test_config: CMakeFiles/test_config.dir/tests/test_config.cc.o
bin/test_config: CMakeFiles/test_config.dir/build.make
bin/test_config: lib/libsylar.so
bin/test_config: /usr/local/lib/libyaml-cpp.a
bin/test_config: CMakeFiles/test_config.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/Web-learning/sylar/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable bin/test_config"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_config.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/test_config.dir/build: bin/test_config

.PHONY : CMakeFiles/test_config.dir/build

CMakeFiles/test_config.dir/requires: CMakeFiles/test_config.dir/tests/test_config.cc.o.requires

.PHONY : CMakeFiles/test_config.dir/requires

CMakeFiles/test_config.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/test_config.dir/cmake_clean.cmake
.PHONY : CMakeFiles/test_config.dir/clean

CMakeFiles/test_config.dir/depend:
	cd /root/Web-learning/sylar && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/Web-learning/sylar /root/Web-learning/sylar /root/Web-learning/sylar /root/Web-learning/sylar /root/Web-learning/sylar/CMakeFiles/test_config.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/test_config.dir/depend

