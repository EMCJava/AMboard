
//
// Created by LYS on 2/16/2026.
//

#include "WindowBase.hxx"
#include "API/WebGPUAPI.hxx"
#include "Interface.hxx"

#include <cassert>
#include <iostream>
#include <vector>

#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <spdlog/spdlog.h>

inline void
GLFWErrorCallback(int error, const char* description)
{
    spdlog::error("GLFW Error {}: {}\n", error, description);
}

void WindowKeyInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    static_cast<CWindowBase*>(glfwGetWindowUserPointer(window))->GetInputManager().RegisterKeyInput(key, scancode, action, mods);
}

void WindowMouseButtonCallback(GLFWwindow* window, int Button, int Action, int Mods)
{
    static_cast<CWindowBase*>(glfwGetWindowUserPointer(window))->GetInputManager().RegisterMouseInput(Button, Action, Mods);
}

void WindowCursorCallback(GLFWwindow* window, double xpos, double ypos)
{
    static_cast<CWindowBase*>(glfwGetWindowUserPointer(window))->GetInputManager().RegisterCursorPosition(xpos, ypos);
}

void WindowCursorScrollCallback(GLFWwindow* window, double, double delta)
{
    static_cast<CWindowBase*>(glfwGetWindowUserPointer(window))->GetInputManager().RegisterCursorScroll(delta);
}

void WindowResizeCallback(GLFWwindow* window, int, int)
{
    static_cast<CWindowBase*>(glfwGetWindowUserPointer(window))->RecreateSurface();
}

/// ===============
/// File Dropping
/// ===============

#ifdef _WIN32

#include <ole2.h>
#include <shellapi.h>
#include <windows.h>

class DropTargetImpl : public IDropTarget {
public:
    CWindowBase* window;
    ULONG refCount = 1;
    HWND hwnd;

    DropTargetImpl(CWindowBase* window, HWND h)
        : window(window)
        , hwnd(h)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG count = --refCount;
        if (count == 0)
            delete this;
        return count;
    }

    // Helper to get local window coordinates
    void GetLocalCoords(POINTL pt, int& x, int& y)
    {
        POINT p = { pt.x, pt.y };
        ScreenToClient(hwnd, &p);
        x = p.x;
        y = p.y;
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        return DragOver(grfKeyState, pt, pdwEffect);
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        int x, y;
        GetLocalCoords(pt, x, y);

        // Ask the user if we are in the drop zone
        if (window->OnFileDrag(x, y)) {
            *pdwEffect = DROPEFFECT_LINK;
        } else {
            *pdwEffect = DROPEFFECT_NONE;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        window->OnFileDragLeave();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        int x, y;
        GetLocalCoords(pt, x, y);

        // Only process if the user allowed the drop at this location
        if (window->OnFileDrag(x, y)) {
            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }

        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg;
        if (SUCCEEDED(pDataObj->GetData(&fmt, &stg))) {
            HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
            UINT count = DragQueryFileA(hDrop, 0xFFFFFFFF, nullptr, 0);

            std::vector<std::string> files;
            for (UINT i = 0; i < count; i++) {
                char path[MAX_PATH];
                DragQueryFileA(hDrop, i, path, MAX_PATH);
                files.push_back(path);
            }

            GlobalUnlock(stg.hGlobal);
            ReleaseStgMedium(&stg);

            window->OnFileDragDrop(x, y, files);
            *pdwEffect = DROPEFFECT_COPY;
        }
        return S_OK;
    }
};

#endif

SRenderContext::SRenderContext() = default;

SRenderContext::~SRenderContext()
{
    RenderPassEncoder.End();

    wgpu::CommandBufferDescriptor cmdBufferDescriptor;
    cmdBufferDescriptor.label = "Command buffer";
    const wgpu::CommandBuffer command = CommandEncoder.Finish(&cmdBufferDescriptor);

    // Finally submit the command queue
    WindowContext->m_Queue.Submit(1, &command);
#ifndef __EMSCRIPTEN__
    (void)WindowContext->m_Surface.Present();
#endif
}

SRenderContext::
operator bool() const noexcept
{
    return WindowContext != nullptr;
}

wgpu::TextureView
CWindowBase::GetNextSurfaceView()
{
    /// Get Surface Texture
    wgpu::SurfaceTexture surfaceTexture;
    m_Surface.GetCurrentTexture(&surfaceTexture);

    /// Failed
    if (surfaceTexture.status > wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
        return nullptr;

    /// Create Texture View
    wgpu::TextureViewDescriptor viewDescriptor;
    viewDescriptor.label = toWgpuStringView("Surface texture view");
    viewDescriptor.dimension = wgpu::TextureViewDimension::e2D;
    return surfaceTexture.texture.CreateView(&viewDescriptor);
}

CWindowBase::CWindowBase()
{
    m_StartTime = std::chrono::steady_clock::now();

    // Move the whole initialization here
    // Open window
    glfwSetErrorCallback(GLFWErrorCallback);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // <-- extra info for glfwCreateWindow
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_Window = { glfwCreateWindow(m_WindowSize.x, m_WindowSize.y, "Learn WebGPU", nullptr, nullptr), [](GLFWwindow* Window) static {
                    if (Window)
                        glfwDestroyWindow(Window);
                    glfwTerminate();
                } };

    glfwSetKeyCallback(m_Window.get(), WindowKeyInputCallback);
    glfwSetMouseButtonCallback(m_Window.get(), WindowMouseButtonCallback);
    glfwSetCursorPosCallback(m_Window.get(), WindowCursorCallback);
    glfwSetScrollCallback(m_Window.get(), WindowCursorScrollCallback);
    glfwSetFramebufferSizeCallback(m_Window.get(), WindowResizeCallback);
    glfwSetWindowUserPointer(m_Window.get(), this);
    glfwFocusWindow(m_Window.get());

    m_InputManager = std::make_unique<CInputManager>(m_Window.get());

    m_WebGpuInstance = wgpu::CreateInstance(nullptr);

    // Get adapter
    std::cout << "Requesting adapter..." << std::endl;
    m_Surface = glfwCreateWindowWGPUSurface(m_WebGpuInstance, m_Window.get());

    WGPURequestAdapterOptions adapterOpts = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    adapterOpts.compatibleSurface = m_Surface.Get();

    WGPUAdapter adapter = requestAdapterSync(m_WebGpuInstance.Get(), &adapterOpts);
    std::cout << "Got adapter: " << adapter << std::endl;

    std::cout << "Requesting device..." << std::endl;
    WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
    // Any name works here, that's your call
    deviceDesc.label = toWgpuStringView("My Device");
    std::vector<WGPUFeatureName> features;
    // No required feature for now
    deviceDesc.requiredFeatureCount = features.size();
    deviceDesc.requiredFeatures = features.data();
    // Make sure 'features' lives until the call to wgpuAdapterRequestDevice!
    WGPULimits requiredLimits = WGPU_LIMITS_INIT;
    // We leave 'requiredLimits' untouched for now
    deviceDesc.requiredLimits = &requiredLimits;
    // Make sure that the 'requiredLimits' variable lives until the call to wgpuAdapterRequestDevice!
    deviceDesc.defaultQueue.label = toWgpuStringView("The Default Queue");
    auto onDeviceLost = [](
                            WGPUDevice const* device,
                            WGPUDeviceLostReason reason,
                            struct WGPUStringView message,
                            void* /* userdata1 */,
                            void* /* userdata2 */
                            ) static {
        // All we do is display a message when the device is lost
        std::cout
            << "Device " << device << " was lost: reason " << reason
            << " (" << toStdStringView(message) << ")"
            << std::endl;
    };
    deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
    deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    auto onDeviceError = [](
                             WGPUDevice const* device,
                             WGPUErrorType type,
                             struct WGPUStringView message,
                             void* /* userdata1 */,
                             void* /* userdata2 */
                             ) static {
        std::cout
            << "Uncaptured error in device " << device << ": type " << type
            << " (" << toStdStringView(message) << ")"
            << std::endl;
    };
    deviceDesc.uncapturedErrorCallbackInfo.callback = onDeviceError;
    // NB: 'device' is now declared at the class level
    m_Device = wgpu::Device::Acquire(requestDeviceSync(m_WebGpuInstance.Get(), adapter, &deviceDesc));
    std::cout << "Got device: " << m_Device.Get() << std::endl;

    {
        m_SurfaceConfig.width = m_WindowSize.x;
        m_SurfaceConfig.height = m_WindowSize.y;
        m_SurfaceConfig.device = m_Device;
        m_SurfaceConfig.presentMode = wgpu::PresentMode::Fifo;
        m_SurfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;

        {
            // We initialize an empty capability struct:
            wgpu::SurfaceCapabilities capabilities;
            m_Surface.GetCapabilities(adapter, &capabilities);
            m_SurfaceFormat = m_SurfaceConfig.format = capabilities.formats[0];
        }

        RecreateSurface();
    }

    // We no longer need to access the adapter
    wgpuAdapterRelease(adapter);

    // The variable 'queue' is now declared at the class level
    // (do NOT prefix this line with 'WGPUQueue' otherwise it'd shadow the class attribute)
    m_Queue = m_Device.GetQueue();

    /// ===============
    /// File Dropping
    /// ===============

    OleInitialize(nullptr); // Required for Windows Drag and Drop
    HWND hwnd = glfwGetWin32Window(m_Window.get());

    // Revoke GLFW's default drop target, and register ours
    RevokeDragDrop(hwnd);
    DropTargetImpl* target = new DropTargetImpl(this, hwnd);
    RegisterDragDrop(hwnd, target);
    m_DropTarget = { target, [this](void* Ptr) {
                        HWND hwnd = glfwGetWin32Window(m_Window.get());
                        RevokeDragDrop(hwnd);
                        ((DropTargetImpl*)Ptr)->Release();
                        OleUninitialize();
                    } };
}

CWindowBase::~CWindowBase() = default;

void CWindowBase::RecreateSurface()
{
    if (!m_Surface) {
        return;
    }

    glfwGetFramebufferSize(m_Window.get(), &m_WindowSize.x, &m_WindowSize.y);
    m_SurfaceConfig.width = static_cast<uint32_t>(m_WindowSize.x);
    m_SurfaceConfig.height = static_cast<uint32_t>(m_WindowSize.y);

    glm::ivec2 PhysicsWindowSize;
    glfwGetWindowSize(m_Window.get(), &PhysicsWindowSize.x, &PhysicsWindowSize.y);
    m_InputManager->SetFrameBufferScale(glm::vec2 { m_WindowSize } / glm::vec2 { PhysicsWindowSize });

    m_Surface.Configure(&m_SurfaceConfig);

    for (auto& Callback : m_OnWindowResizes)
        if (Callback)
            Callback(this);
}

CInputManager&
CWindowBase::GetInputManager() const noexcept
{
    assert(m_InputManager);
    return *m_InputManager;
}

CWindowBase::EWindowEventState CWindowBase::ProcessEvent()
{
    const auto NowTime = std::chrono::steady_clock::now();

    GlobalFrameCounter++;
    GlobalGameSecond = std::chrono::duration<float>(NowTime - m_StartTime).count();

    if (glfwWindowShouldClose(m_Window.get()))
        return EWindowEventState::Closed;

    m_InputManager->ClearEvents();
    glfwPollEvents();
    m_InputManager->AdvanceFrame();

    if (glfwGetWindowAttrib(m_Window.get(), GLFW_ICONIFIED) != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds { 10 });
        m_LastUpdateTime = std::chrono::steady_clock::now();
        return EWindowEventState::Minimized;
    }

    m_WebGpuInstance.ProcessEvents();

    m_DeltaTime = std::chrono::duration<float> { NowTime - m_LastUpdateTime }.count();
    m_LastUpdateTime = NowTime;

    return EWindowEventState::Normal;
}

SRenderContext
CWindowBase::GetRenderContext(const wgpu::TextureView& DepthTextureView)
{
    using namespace wgpu;

    SRenderContext Result;

    // Get the next target texture view
    auto targetView = GetNextSurfaceView();
    if (!targetView)
        return Result; // no surface texture, we skip this frame

    CommandEncoderDescriptor encoderDesc;
    encoderDesc.label = "My command encoder";
    Result.CommandEncoder = m_Device.CreateCommandEncoder(&encoderDesc);

    RenderPassColorAttachment colorAttachment;
    colorAttachment.view = targetView;
    colorAttachment.loadOp = LoadOp::Clear;
    colorAttachment.storeOp = StoreOp::Store;
    colorAttachment.clearValue = { 1.0, 0.8, 0.55, 1.0 };

    RenderPassDepthStencilAttachment depthStencilAttachment;
    depthStencilAttachment.view = DepthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = StoreOp::Store;
    depthStencilAttachment.depthReadOnly = false;

    depthStencilAttachment.stencilReadOnly = true;

    RenderPassDescriptor renderPassDesc;
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;
    assert(DepthTextureView);
    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

    Result.RenderPassEncoder = Result.CommandEncoder.BeginRenderPass(&renderPassDesc);

    Result.WindowContext = this;

    return Result;
}

bool CWindowBase::WindowShouldClose() const noexcept
{
    return m_Window ? glfwWindowShouldClose(m_Window.get()) : true;
}

void CWindowBase::StartFrame()
{
    m_LastUpdateTime = std::chrono::steady_clock::now();
}

void CWindowBase::UpdateLoop()
{
    while (!WindowShouldClose())
        GetRenderContext();
}