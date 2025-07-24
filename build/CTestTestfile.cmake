# CMake generated Testfile for 
# Source directory: D:/pub_sub
# Build directory: D:/pub_sub/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_spsc]=] "D:/pub_sub/build/test_spsc.exe")
set_tests_properties([=[test_spsc]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/pub_sub/CMakeLists.txt;93;add_test;D:/pub_sub/CMakeLists.txt;0;")
subdirs("_deps/googletest-build")
subdirs("tests")
