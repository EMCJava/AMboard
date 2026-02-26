find_package(spdlog CONFIG REQUIRED)
create_library(CustomNodeManager P_DEPS spdlog::spdlog)

add_library(Branching SHARED BranchingNode.cxx)
target_compile_definitions(Branching PRIVATE MACRO_API_IMPORTS)
target_link_libraries(Branching PRIVATE MacroSharedLib)