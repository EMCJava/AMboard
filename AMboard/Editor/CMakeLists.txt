find_package(glm CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS bimap)

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

        GridPipline
        NodeRenderer

        ExecuteNode
        FlowPin
        DataPin)