cmake_minimum_required(VERSION 3.16)
project(TBleStateMachine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy / IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strict compile flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wnon-virtual-dtor
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
)

# Enable testing
enable_testing()

# Subdirectories
add_subdirectory(src)
add_subdirectory(test)




