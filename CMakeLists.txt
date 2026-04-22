cmake_minimum_required(VERSION 3.20)
project(iot_sampler)

set(CMAKE_CXX_STANDARD 17)

# KissFFT
add_library(kissfft
        third_party/kissfft/kiss_fft.c
        third_party/kissfft/kiss_fftr.c
)
target_include_directories(kissfft PUBLIC
        third_party/kissfft
)

# Signal test
add_executable(test_signal
        src/signal.cpp
        tests/test_signal.cpp
)
target_link_libraries(test_signal m)
target_include_directories(test_signal PRIVATE src)

# FFT test
add_executable(test_fft
        src/signal.cpp
        src/fft.cpp
        tests/test_fft.cpp
)
target_link_libraries(test_fft m kissfft)
target_include_directories(test_fft PRIVATE src)