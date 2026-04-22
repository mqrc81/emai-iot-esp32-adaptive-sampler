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

# Sampler test
add_executable(test_sampler
        src/signal.cpp
        src/fft.cpp
        src/sampler.cpp
        tests/test_sampler.cpp
)
target_link_libraries(test_sampler m kissfft)
target_include_directories(test_sampler PRIVATE src)

# MQTT test
add_executable(test_mqtt
        src/signal.cpp
        src/fft.cpp
        src/sampler.cpp
        src/mqtt_client.cpp
        tests/test_mqtt.cpp
)
target_link_libraries(test_mqtt m kissfft mosquitto)
target_include_directories(test_mqtt PRIVATE src /usr/local/include)
target_link_directories(test_mqtt PRIVATE /usr/local/lib)

# Signal noisy test
add_executable(test_signal_noisy
        src/signal.cpp
        tests/test_signal_noisy.cpp
)
target_link_libraries(test_signal_noisy m)
target_include_directories(test_signal_noisy PRIVATE src)

# Filter test
add_executable(test_filter
        src/signal.cpp
        src/filter.cpp
        tests/test_filter.cpp
)
target_link_libraries(test_filter m)
target_include_directories(test_filter PRIVATE src)

# FFT contamination test
add_executable(test_fft_contamination
        src/signal.cpp
        src/fft.cpp
        src/filter.cpp
        tests/test_fft_contamination.cpp
)
target_link_libraries(test_fft_contamination m kissfft)
target_include_directories(test_fft_contamination PRIVATE src)

# Test window tradeoff
add_executable(test_window_tradeoff
        src/signal.cpp
        src/filter.cpp
        tests/test_window_tradeoff.cpp
)
target_link_libraries(test_window_tradeoff m)
target_include_directories(test_window_tradeoff PRIVATE src)

# Test signals comparison
add_executable(test_signals_comparison
        src/signal.cpp
        src/fft.cpp
        src/sampler.cpp
        tests/test_signals_comparison.cpp
)
target_link_libraries(test_signals_comparison m kissfft)
target_include_directories(test_signals_comparison PRIVATE src)