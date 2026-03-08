find_package(spdlog CONFIG REQUIRED)
create_library(CustomNodeManager P_DEPS spdlog::spdlog)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/Zc:preprocessor)
endif ()

add_library(CommonNode SHARED CommonNode.cxx)
target_compile_definitions(CommonNode PRIVATE MACRO_API_IMPORTS)
target_link_libraries(CommonNode PRIVATE MacroSharedLib spdlog::spdlog ImGui.lib InputService.lib)

#Copy CommonNode AND its dependencies
add_custom_command(TARGET CommonNode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/NodeExts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:CommonNode>            # Copies CommonNode.dll / .so
        $<TARGET_RUNTIME_DLLS:CommonNode>    # Copies all dependency DLLs
        ${CMAKE_SOURCE_DIR}/NodeExts         # Destination folder
        COMMAND_EXPAND_LISTS                 # CRITICAL: Expands the list of DLLs properly
        VERBATIM
)