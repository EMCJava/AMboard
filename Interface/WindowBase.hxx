//
// Created by LYS on 2/16/2026.
//
#pragma once

#include "InputManager.hxx"

#include <chrono>
#include <functional>
#include <list>
#include <memory>

#include <glm/vec2.hpp>

#include <dawn/webgpu_cpp.h>

class CWindowBase;
struct SRenderContext {
    SRenderContext();
    ~SRenderContext();

    wgpu::TextureView TextureView;
    wgpu::CommandEncoder CommandEncoder;
    wgpu::RenderPassEncoder RenderPassEncoder;

    [[nodiscard]] operator bool() const noexcept;

private:
    friend CWindowBase;
    CWindowBase* WindowContext = nullptr;
};

class CWindowBase {
    wgpu::TextureView GetNextSurfaceView();

public:
    CWindowBase();
    ~CWindowBase();

    CWindowBase(const CWindowBase&) = delete;
    CWindowBase& operator=(const CWindowBase&) = delete;
    CWindowBase(CWindowBase&&) = delete;
    CWindowBase& operator=(CWindowBase&&) = delete;

    auto GetWindowRatio() const noexcept { return static_cast<float>(m_WindowSize.x) / m_WindowSize.y; }
    auto& GetWindowSize() const noexcept { return m_WindowSize; }
    auto GetWindowHandle() const noexcept { return m_Window.get(); }

    using WindowResizeCallbacks = std::list<std::function<void(CWindowBase* Window)>>;
    auto AddOnWindowResizes(auto&& Callback) { return m_OnWindowResizes.emplace(m_OnWindowResizes.end(), std::forward<decltype(Callback)>(Callback)); }
    auto EraseOnWindowResizes(auto&& Iter) { return m_OnWindowResizes.erase(Iter); }

    // Mouse is dragging a file over the window.
    // Return TRUE to allow drop. Return FALSE to deny.
    virtual bool OnFileDrag(int WindowX, int WindowY) { return false; }
    virtual void OnFileDragLeave() { }
    virtual void OnFileDragDrop(int WindowX, int WindowY, const std::vector<std::string>& Files) { }

    virtual void RecreateSurface();

    float GetDeltaTime() const noexcept { return m_DeltaTime; }
    CInputManager& GetInputManager() const noexcept;

    enum class EWindowEventState {
        Normal,
        Minimized,
        Closed
    };
    virtual EWindowEventState ProcessEvent();
    SRenderContext GetRenderContext(const wgpu::TextureView& DepthTextureView = { });

    [[nodiscard]] const wgpu::Device& GetDevice() const noexcept { return m_Device; }
    [[nodiscard]] const wgpu::Queue& GetQueue() const noexcept { return m_Queue; }
    [[nodiscard]] wgpu::TextureFormat GetSurfaceFormat() const noexcept { return m_SurfaceFormat; }
    [[nodiscard]] wgpu::TextureFormat GetDepthFormat() const noexcept { return m_DepthTextureFormat; }

    bool WindowShouldClose() const noexcept;
    void StartFrame(); // Just reset the last updated time
    void UpdateLoop();

protected:
    glm::ivec2 m_WindowSize { 1920, 1080 };

    wgpu::Instance m_WebGpuInstance;
    std::unique_ptr<struct GLFWwindow, std::function<void(GLFWwindow*)>> m_Window;
    wgpu::Device m_Device;
    wgpu::Surface m_Surface;
    wgpu::Queue m_Queue;

    wgpu::SurfaceConfiguration m_SurfaceConfig;
    wgpu::TextureFormat m_DepthTextureFormat = wgpu::TextureFormat::Depth32Float;
    wgpu::TextureFormat m_SurfaceFormat;

    WindowResizeCallbacks m_OnWindowResizes;

    std::unique_ptr<CInputManager> m_InputManager;

    std::chrono::time_point<std::chrono::steady_clock> m_StartTime;
    std::chrono::time_point<std::chrono::steady_clock> m_LastUpdateTime;

    std::unique_ptr<void, std::function<void(void*)>> m_DropTarget;

    float m_DeltaTime = 0;

    friend SRenderContext;
};