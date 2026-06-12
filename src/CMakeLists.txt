# Header-only library: BLE state machine core
add_library(tble INTERFACE)

target_include_directories(tble INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(tble INTERFACE cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(tble INTERFACE Threads::Threads)

# Demo executable
add_executable(tble_demo main.cpp)
target_link_libraries(tble_demo PRIVATE tble)