find_package(glm CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS bimap)
find_package(yaml-cpp CONFIG REQUIRED)

# |======================================================
# |                       ImGui
# |======================================================

include(FetchContent)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.6
)
FetchContent_MakeAvailable(imgui)

create_library(ImGui
        SRCS
        # Dear ImGui Backend files
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
        # Dear ImGui files
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp

        INCLUDES
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends

        P_DEPS
        WindowBase # dawn
)
target_compile_definitions(ImGui.lib PUBLIC "IMGUI_IMPL_WEBGPU_BACKEND_DAWN")

# |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# |                       ImGui
# |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_LEFT_HANDED)

add_subdirectory(NodeRender)

create_library(GridPipline DEPS WindowBase RenderPipeline)
create_library(
        BoardEditor

        DEPS
        WindowBase
        Boost::bimap

        P_DEPS
        Assertions
        CustomNodeManager

        glm::glm
        nfd::nfd
        yaml-cpp::yaml-cpp
        ImGui

        GridPipline
        NodeRenderer

        ExecuteNode
        FlowPin
        DataPin)