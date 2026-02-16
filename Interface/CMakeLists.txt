find_package(glm CONFIG REQUIRED)
find_package(Dawn CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_LEFT_HANDED)

add_subdirectory(API)

add_compile_definitions(_GLFW_WIN32)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_LEFT_HANDED)

create_library(InputManager DEPS glm::glm P_DEPS spdlog::spdlog)
create_library(WindowBase DEPS InputManager dawn::webgpu_dawn glm::glm P_DEPS spdlog::spdlog glfw WebGPUAPI)
