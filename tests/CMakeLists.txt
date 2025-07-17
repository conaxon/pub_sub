add_executable(test_message_queue tests/test_message_quieue.cpp)

target_link_libraries(test_message_queue PRIVATE core GTest::gtest_main)

include(GoogleTest)

gtest_discover_tests(test_message_queue)