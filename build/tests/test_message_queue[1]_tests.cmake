add_test([=[MessageQueueTest.PushAndPop]=]  D:/pub_sub/build/tests/test_message_queue.exe [==[--gtest_filter=MessageQueueTest.PushAndPop]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MessageQueueTest.PushAndPop]=]  PROPERTIES WORKING_DIRECTORY D:/pub_sub/build/tests SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  test_message_queue_TESTS MessageQueueTest.PushAndPop)
