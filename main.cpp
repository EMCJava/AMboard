#include <Interface/Pipline/DepthTexture.hxx>

#include <AMboard/Editor/BoardEditor.hxx>

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