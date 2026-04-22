cmake_minimum_required(VERSION 3.20)
project(iot_sampler)

set(CMAKE_CXX_STANDARD 17)

# Signal generation test
add_executable(test_signal
        src/signal.cpp
        tests/test_signal.cpp
)
target_link_libraries(test_signal m)
target_include_directories(test_signal PRIVATE src)