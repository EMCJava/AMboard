cmake_minimum_required(VERSION 4.1)
project(AMB)

set(CMAKE_CXX_STANDARD 23)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(library)

include_directories(.)
add_subdirectory(Util)
add_subdirectory(Interface)
add_subdirectory(AMBoard)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_LEFT_HANDED)


add_executable(AMB main.cpp)

target_link_libraries(
        AMB
        ExecuteNode.lib
        BaseNode.lib
        DataPin.lib
        FlowPin.lib
        Pin.lib

        EventHooker.lib

        BoardEditor.lib
        DepthTexture.lib
        RenderPipeline.lib
)
