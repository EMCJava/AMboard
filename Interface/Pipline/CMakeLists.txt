create_library(RenderPipeline DEPS dawn::webgpu_dawn P_DEPS WindowBase)
create_library(DepthTexture P_DEPS WindowBase)
create_library(TextPipline DEPS RenderPipeline P_DEPS TextRenderMeta)
create_library(TextRenderMeta INTERFACE DEPS glm::glm)