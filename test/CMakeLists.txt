 # tests/CMakeLists.txt
add_executable(test_ranges test_ranges.cpp) # Example test sources

add_test(NAME RangesTest COMMAND test_ranges) # Register the test 