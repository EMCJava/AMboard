cmake_minimum_required(VERSION 4.1)
project(AMboard)

set(CMAKE_CXX_STANDARD 23)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(library)

include_directories(.)
add_subdirectory(Macro)

add_executable(AMboard main.cpp)

target_link_libraries(
        AMboard
        ExecuteNode.lib
        BaseNode.lib
        FlowPin.lib
        Pin.lib
)
