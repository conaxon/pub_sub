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
if (WIN32)
  target_link_libraries(asio INTERFACE Threads::Threads Ws2_32 Mswsock)
else()
  target_link_libraries(asio INTERFACE Threads::Threads)
endif()

# Pull in GoogleTest for unit testing
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Core interface library for shared headers and dependencies
add_library(core INTERFACE)
file(GLOB_RECURSE CORE_HDR_DIRS 
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp)

# Convert to full paths, strip filenames to get directories:
set(_unique_dirs "")
foreach(h ${CORE_HDR_DIRS})
  get_filename_component(dir ${h} PATH)
  list(APPEND _unique_dirs ${CMAKE_CURRENT_SOURCE_DIR}/${dir})
endforeach()
list(REMOVE_DUPLICATES _unique_dirs)

target_include_directories(core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${_unique_dirs})
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