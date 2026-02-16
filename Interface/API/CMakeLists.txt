create_library(WebGPUAPI DEPS dawn::webgpu_dawn P_DEPS glfw)
target_compile_definitions(WebGPUAPI.lib PRIVATE _GLFW_WIN32)
