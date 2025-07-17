cmake_minimum_required(VERSION 3.20)
project(pubsub, LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    $<$<CXX_COMPILER_ID:GNU,Clang>:Wall -Wextra -Wpeedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX/
)

include(FetchContent)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-24-1
)
FetchContent_MakeAvailable(asio)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

target_interface_libraries(core INTERFACE Asio::asio)

add_executable(broker src/broker.cpp)
target_link_libraries(broker PRIVATE core)

add_executable(pub_client src/pub_client.cpp)
target_link_libraries(pub_client PRIVATE core)

add_executable(sub_client src/sub_client.cpp)
target_link_libraries(sub_client PRIVATE core)

enable_testing()
add_subdirectory(tests)
