# The arpeggiator hosted in Oscairn's engine (RFC 0032 step 2). Built by hand, not in CI: it needs an
# Oscairn source tree, which it builds into this build folder.
#   cmake -S tests/oscairn -B build-oscairn -G Ninja -DCMAKE_BUILD_TYPE=Release -DOSCAIRN_SOURCE=/path/to/oscairn
#   cmake --build build-oscairn && build-oscairn/arp-hosted-test ARP.clap WORKER PUSHER.clap [ARCHIVE.olib OCTET.clap]
cmake_minimum_required(VERSION 3.20)
project(arp_oscairn_hosted CXX C)
set(CMAKE_CXX_STANDARD 20)
set(OSCAIRN_SOURCE "" CACHE PATH "Oscairn source tree")
if(NOT OSCAIRN_SOURCE)
    message(FATAL_ERROR "set OSCAIRN_SOURCE")
endif()
add_subdirectory(${OSCAIRN_SOURCE} oscairn EXCLUDE_FROM_ALL)
add_library(arp-pusher MODULE pusher.cpp)
target_link_libraries(arp-pusher PRIVATE oscairn_clap_headers)
set_target_properties(arp-pusher PROPERTIES PREFIX "" SUFFIX ".clap")
add_executable(arp-hosted-test hosted.cpp)
target_link_libraries(arp-hosted-test PRIVATE oscairn_control)
add_dependencies(arp-hosted-test arp-pusher oscairn-plugin-worker)
