find_package(spdlog CONFIG REQUIRED)
create_library(CustomNodeManager P_DEPS spdlog::spdlog)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/Zc:preprocessor)
endif ()

add_library(ExtCommonNode SHARED CommonNode.cxx)
target_compile_definitions(ExtCommonNode PRIVATE MACRO_API_IMPORTS)
target_link_libraries(ExtCommonNode PRIVATE MacroSharedLib spdlog::spdlog ImGui.lib InputService.lib InputDispatcher.lib)

if (APPLE)
    target_link_libraries(ExtCommonNode PRIVATE "-framework Carbon")
endif (APPLE)

find_package(OpenCV REQUIRED)

add_library(ExtWindowUtil SHARED WindowUtil.cxx ${ImGui_Src}/misc/cpp/imgui_stdlib.cpp)
target_compile_definitions(ExtWindowUtil PRIVATE MACRO_API_IMPORTS)
target_link_libraries(ExtWindowUtil PRIVATE MacroSharedLib ImGui.lib ${OpenCV_LIBS})
target_include_directories(ExtWindowUtil PRIVATE ${OpenCV_INCLUDE_DIRS})

add_subdirectory(CV)

set(EXT_LIBRARIES
        ExtCommonNode
        ExtWindowUtil
)
set(DEST_FOLDER "${CMAKE_SOURCE_DIR}/NodeExts")
foreach(TARGET_NAME IN LISTS EXT_LIBRARIES)
    set_target_properties(${TARGET_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${DEST_FOLDER}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DEST_FOLDER}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DEST_FOLDER}"
    )
endforeach()