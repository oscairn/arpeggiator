cmake_minimum_required(VERSION 3.20)
project(oscairn_arpeggiator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(arp_engine STATIC src/phrase.cpp src/player.cpp)
target_include_directories(arp_engine PUBLIC src)
target_compile_options(arp_engine PRIVATE -Wall -Wextra)

add_executable(arp-render tools/arp_render.cpp)
target_link_libraries(arp-render PRIVATE arp_engine)

enable_testing()
add_executable(arp_tests tests/test_main.cpp tests/test_player.cpp tests/test_phrase.cpp)
target_link_libraries(arp_tests PRIVATE arp_engine)
add_test(NAME engine COMMAND arp_tests)
