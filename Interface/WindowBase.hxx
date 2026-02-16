//
// Created by LYS on 2/16/2026.
//
#pragma once

#include "InputManager.hxx"

#include <chrono>
#include <functional>
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

    auto AddOnWindowResizes(auto&& Callback) { return m_OnWindowResizes.emplace_back(std::forward<decltype(Callback)>(Callback)); }

    void RecreateSurface();

    float GetDeltaTime() const noexcept { return m_DeltaTime; }
    CInputManager& GetInputManager() const noexcept;

    bool ProcessEvent();
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

    std::list<std::function<void()>> m_OnWindowResizes;

    std::unique_ptr<CInputManager> m_InputManager;

    std::chrono::time_point<std::chrono::steady_clock> m_LastUpdateTime;

    float m_DeltaTime = 0;

    friend SRenderContext;
};