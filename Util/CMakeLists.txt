find_package(spdlog CONFIG REQUIRED)
find_package(cpptrace CONFIG REQUIRED)

create_library(Assertions INTERFACE DEPS spdlog::spdlog cpptrace::cpptrace)