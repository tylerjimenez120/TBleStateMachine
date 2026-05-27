# Header-only library: BLE state machine core
add_library(tble INTERFACE)

target_include_directories(tble INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(tble INTERFACE cxx_std_17)

# std::thread / std::mutex need pthread on Linux
find_package(Threads REQUIRED)
target_link_libraries(tble INTERFACE Threads::Threads)