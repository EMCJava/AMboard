create_library(Pin P_DEPS Assertions)
create_library(DataPin DEPS Pin)
create_library(FlowPin DEPS Pin)
create_library(BaseNode DEPS Pin)
create_library(ExecuteNode DEPS BaseNode FlowPin)

create_library(MacroSharedLib SHARED RSRCS *.hxx *.cxx P_DEPS Assertions)
target_compile_definitions(MacroSharedLib PRIVATE MACRO_API_EXPORTS)
