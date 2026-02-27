find_package(glm CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS bimap)
find_package(yaml-cpp CONFIG REQUIRED)

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

        GridPipline
        NodeRenderer

        ExecuteNode
        FlowPin
        DataPin)