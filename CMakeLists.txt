cmake_minimum_required(VERSION 3.20)
project(oscairn_arpeggiator VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(clap_headers INTERFACE)
target_include_directories(clap_headers INTERFACE third_party/clap/include third_party/oscairn-clap/include)

# The phrase model and player, shared by the plugin, the tests and the tools.
add_library(arp_engine STATIC src/phrase.cpp src/player.cpp src/record.cpp)
target_include_directories(arp_engine PUBLIC src)
target_compile_options(arp_engine PRIVATE -Wall -Wextra)

add_library(oscairn_arpeggiator MODULE src/plugin.cpp)
target_link_libraries(oscairn_arpeggiator PRIVATE arp_engine clap_headers)
target_compile_options(oscairn_arpeggiator PRIVATE -Wall -Wextra)
set_target_properties(oscairn_arpeggiator PROPERTIES PREFIX "" SUFFIX ".clap" OUTPUT_NAME oscairn-arpeggiator
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
install(TARGETS oscairn_arpeggiator LIBRARY DESTINATION lib/clap)

add_executable(arp-render tools/arp_render.cpp)
target_link_libraries(arp-render PRIVATE arp_engine)

enable_testing()
add_executable(arp_tests tests/test_main.cpp tests/test_player.cpp tests/test_phrase.cpp tests/test_record.cpp)
target_link_libraries(arp_tests PRIVATE arp_engine)
add_test(NAME engine COMMAND arp_tests)

add_executable(plugin_tests tests/test_plugin.cpp)
target_link_libraries(plugin_tests PRIVATE clap_headers ${CMAKE_DL_LIBS})
add_dependencies(plugin_tests oscairn_arpeggiator)
add_test(NAME plugin COMMAND plugin_tests $<TARGET_FILE:oscairn_arpeggiator>)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME midi_import COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_midi_to_phrase.py)
endif()
