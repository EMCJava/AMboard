find_package(spdlog CONFIG REQUIRED)
create_library(CustomNodeManager P_DEPS spdlog::spdlog)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/Zc:preprocessor)
endif()

add_library(CommonNode SHARED CommonNode.cxx)
target_compile_definitions(CommonNode PRIVATE MACRO_API_IMPORTS)
target_link_libraries(CommonNode PRIVATE MacroSharedLib)