add_library(ExtScreenPattern SHARED ScreenPattern.cxx ${ImGui_Src}/misc/cpp/imgui_stdlib.cpp)
target_compile_definitions(ExtScreenPattern PRIVATE MACRO_API_IMPORTS)
target_link_libraries(ExtScreenPattern PRIVATE MacroSharedLib ImGui.lib spdlog::spdlog)

target_link_libraries(ExtScreenPattern PRIVATE ${OpenCV_LIBS})
target_include_directories(ExtScreenPattern PRIVATE ${OpenCV_INCLUDE_DIRS})

find_package(OpenCV REQUIRED)

set(EXT_LIBRARIES
        ExtScreenPattern
)
set(DEST_FOLDER "${CMAKE_SOURCE_DIR}/NodeExts")
foreach(TARGET_NAME IN LISTS EXT_LIBRARIES)
    set_target_properties(${TARGET_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${DEST_FOLDER}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DEST_FOLDER}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DEST_FOLDER}"
    )
endforeach()