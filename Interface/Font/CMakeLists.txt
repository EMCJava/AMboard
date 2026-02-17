find_package(harfbuzz CONFIG REQUIRED)
find_package(Freetype REQUIRED)

create_library(Font DEPS glm::glm dawn::webgpu_dawn P_DEPS WindowBase Freetype::Freetype harfbuzz::harfbuzz Assertions)
create_library(TextRenderSystem DEPS DynamicVertexBuffer TextRenderMeta P_DEPS Font TextPipline WindowBase Assertions harfbuzz::harfbuzz)