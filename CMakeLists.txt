# File: CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(pubsub_demo LANGUAGES CXX)

# Use C++20, disable compiler extensions
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
elseif (MSVC)
    add_compile_options(/W4 /WX)
endif()

# Pull in standalone Asio (header-only) via FetchContent
include(FetchContent)
FetchContent_Declare(
  asio_src
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-34-2
)
FetchContent_MakeAvailable(asio_src)

# Threads support (for Asio)
find_package(Threads REQUIRED)

# Define Asio interface target
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE
  ${asio_src_SOURCE_DIR}/asio/include
)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
target_link_libraries(asio INTERFACE Threads::Threads)

# Pull in GoogleTest for unit testing
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Core interface library for shared headers and dependencies
add_library(core INTERFACE)
target_include_directories(core INTERFACE
${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/core
)
target_link_libraries(core INTERFACE asio)

# Broker and client executables
add_executable(broker src/broker.cpp)
target_link_libraries(broker PRIVATE core)

add_executable(pub_client src/pub_client.cpp)
target_link_libraries(pub_client PRIVATE core)

add_executable(sub_client src/sub_client.cpp)
target_link_libraries(sub_client PRIVATE core)

# Enable testing and add tests directory
enable_testing()
add_subdirectory(tests)