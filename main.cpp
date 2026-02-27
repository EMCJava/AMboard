#include <Interface/Pipline/DepthTexture.hxx>
#include <Interface/Pipline/RenderPipeline.hxx>

#include <Interface/Font/TextRenderSystem.hxx>

#include <AMboard/Editor/BoardEditor.hxx>

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>
#include <AMboard/Macro/FlowPin.hxx>

#include <chrono>
#include <iostream>

int main()
{

    CBoardEditor Window;

    CDepthTexture DepthTexture { Window };

    Window.StartFrame();
    while (true) {

        const auto EventState = Window.ProcessEvent();
        if (EventState == CWindowBase::EWindowEventState::Closed)
            break;
        if (EventState == CWindowBase::EWindowEventState::Minimized)
            continue;

        if (const auto RenderContext = Window.GetRenderContext(DepthTexture)) {
            Window.RenderBoard(RenderContext);
        }
    }

    return 0;
}
