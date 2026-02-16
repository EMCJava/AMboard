//
// Created by LYS on 12/14/2025.
//

#include "WebGPUAPI.hxx"

#include <cassert>
#include <iostream>

#include <dawn/webgpu.h>

#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#define GLFW_EXPOSE_NATIVE_EMSCRIPTEN
#ifndef GLFW_PLATFORM_EMSCRIPTEN // not defined in older versions of emscripten
#define GLFW_PLATFORM_EMSCRIPTEN 0
#endif
#else // __EMSCRIPTEN__
#ifdef _GLFW_X11
#define GLFW_EXPOSE_NATIVE_X11
#endif
#endif
#ifdef _GLFW_WAYLAND
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#ifdef _GLFW_COCOA
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#ifdef _GLFW_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#ifdef GLFW_EXPOSE_NATIVE_COCOA
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#ifndef __EMSCRIPTEN__
#include <GLFW/glfw3native.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else // __EMSCRIPTEN__
#include <chrono>
#include <thread>
#endif // __EMSCRIPTEN__

std::string_view
toStdStringView(WGPUStringView wgpuStringView)
{
    return wgpuStringView.data == nullptr
        ? std::string_view()
        : wgpuStringView.length == WGPU_STRLEN
        ? std::string_view(wgpuStringView.data)
        : std::string_view(
              wgpuStringView.data,
              wgpuStringView.length);
}

WGPUStringView
toWgpuStringView(std::string_view stdStringView)
{
    return { stdStringView.data(), stdStringView.size() };
}

WGPUStringView
toWgpuStringView(const char* cString)
{
    return { cString, WGPU_STRLEN };
}

void sleepForMilliseconds(unsigned int milliseconds)
{
#ifdef __EMSCRIPTEN__
    emscripten_sleep(milliseconds);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}
// All utility functions are regrouped here
/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapterSync(options);
 * is roughly equivalent to the JavaScript
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter
requestAdapterSync(WGPUInstance instance,
    WGPURequestAdapterOptions const* options)
{
    // A simple structure holding the local information shared with the
    // onAdapterRequestEnded callback.
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    // Callback called by wgpuInstanceRequestAdapter when the request returns
    // This is a C++ lambda function, but could be any function defined in the
    // global scope. It must be non-capturing (the brackets [] are empty) so
    // that it behaves like a regular C function pointer, which is what
    // wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
    // is to convey what we want to capture through the userdata1 pointer,
    // provided as the last argument of wgpuInstanceRequestAdapter and received
    // by the callback as its last argument.
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status,
                                     WGPUAdapter adapter,
                                     WGPUStringView message,
                                     void* userdata1,
                                     void* /* userdata2 */
                                     ) static {
        UserData& userData = *reinterpret_cast<UserData*>(userdata1);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = adapter;
        } else {
            std::cerr << "Error while requesting adapter: "
                      << toStdStringView(message) << std::endl;
        }
        userData.requestEnded = true;
    };

    // Build the callback info
    WGPURequestAdapterCallbackInfo callbackInfo = {
        /* nextInChain = */ nullptr,
        /* mode = */ WGPUCallbackMode_AllowProcessEvents,
        /* callback = */ onAdapterRequestEnded,
        /* userdata1 = */ &userData,
        /* userdata2 = */ nullptr
    };

    // Call to the WebGPU request adapter procedure
    wgpuInstanceRequestAdapter(instance, options, callbackInfo);

    // We wait until userData.requestEnded gets true

    // Hand the execution to the WebGPU instance so that it can check for
    // pending async operations, in which case it invokes our callbacks.
    // NB: We test once before the loop not to wait for 200ms in case it is
    // already ready
    wgpuInstanceProcessEvents(instance);

    while (!userData.requestEnded) {
        // Waiting for 200 ms to avoid asking too often to process events
        sleepForMilliseconds(200);

        wgpuInstanceProcessEvents(instance);
    }

    return userData.adapter;
}
void inspectAdapter(WGPUAdapter adapter)
{
    WGPULimits supportedLimits = { };
    supportedLimits.nextInChain = nullptr;

    bool success = wgpuAdapterGetLimits(adapter, &supportedLimits) == WGPUStatus_Success;

    if (success) {
        std::cout << "Adapter limits:" << std::endl;
        std::cout << " - maxTextureDimension1D: "
                  << supportedLimits.maxTextureDimension1D << std::endl;
        std::cout << " - maxTextureDimension2D: "
                  << supportedLimits.maxTextureDimension2D << std::endl;
        std::cout << " - maxTextureDimension3D: "
                  << supportedLimits.maxTextureDimension3D << std::endl;
        std::cout << " - maxTextureArrayLayers: "
                  << supportedLimits.maxTextureArrayLayers << std::endl;
    }
    // Prepare the struct where features will be listed
    WGPUSupportedFeatures features;

    // Get adapter features. This may allocate memory that we must later free with
    // wgpuSupportedFeaturesFreeMembers()
    wgpuAdapterGetFeatures(adapter, &features);

    std::cout << "Adapter features:" << std::endl;
    std::cout << std::hex; // Write integers as hexadecimal to ease comparison
                           // with webgpu.h literals
    for (size_t i = 0; i < features.featureCount; ++i) {
        std::cout << " - 0x" << features.features[i] << std::endl;
    }
    std::cout << std::dec; // Restore decimal numbers

    // Free the memory that had potentially been allocated by
    // wgpuAdapterGetFeatures()
    wgpuSupportedFeaturesFreeMembers(features);
    // One shall no longer use features beyond this line.
    WGPUAdapterInfo properties;
    properties.nextInChain = nullptr;
    wgpuAdapterGetInfo(adapter, &properties);
    std::cout << "Adapter properties:" << std::endl;
    std::cout << " - vendorID: " << properties.vendorID << std::endl;
    std::cout << " - vendorName: " << toStdStringView(properties.vendor)
              << std::endl;
    std::cout << " - architecture: " << toStdStringView(properties.architecture)
              << std::endl;
    std::cout << " - deviceID: " << properties.deviceID << std::endl;
    std::cout << " - name: " << toStdStringView(properties.device) << std::endl;
    std::cout << " - driverDescription: "
              << toStdStringView(properties.description) << std::endl;
    std::cout << std::hex;
    std::cout << " - adapterType: 0x" << properties.adapterType << std::endl;
    std::cout << " - backendType: 0x" << properties.backendType << std::endl;
    std::cout << std::dec; // Restore decimal numbers
    wgpuAdapterInfoFreeMembers(properties);
}
/**
 * Utility function to get a WebGPU device, so that
 *     WGPUDevice device = requestDeviceSync(adapter, options);
 * is roughly equivalent to
 *     const device = await adapter.requestDevice(descriptor);
 * It is very similar to requestAdapter
 */
WGPUDevice
requestDeviceSync(WGPUInstance instance,
    WGPUAdapter adapter,
    WGPUDeviceDescriptor const* descriptor)
{
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    // The callback
    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status,
                                    WGPUDevice device,
                                    WGPUStringView message,
                                    void* userdata1,
                                    void* /* userdata2 */
                                    ) static {
        UserData& userData = *reinterpret_cast<UserData*>(userdata1);
        if (status == WGPURequestDeviceStatus_Success) {
            userData.device = device;
        } else {
            std::cerr << "Error while requesting device: " << toStdStringView(message)
                      << std::endl;
        }
        userData.requestEnded = true;
    };

    // Build the callback info
    WGPURequestDeviceCallbackInfo callbackInfo = {
        /* nextInChain = */ nullptr,
        /* mode = */ WGPUCallbackMode_AllowProcessEvents,
        /* callback = */ onDeviceRequestEnded,
        /* userdata1 = */ &userData,
        /* userdata2 = */ nullptr
    };

    // Call to the WebGPU request adapter procedure
    wgpuAdapterRequestDevice(adapter, descriptor, callbackInfo);

    // Hand the execution to the WebGPU instance until the request ended
    wgpuInstanceProcessEvents(instance);
    while (!userData.requestEnded) {
        sleepForMilliseconds(200);
        wgpuInstanceProcessEvents(instance);
    }

    return userData.device;
}
// We create a utility function to inspect the device:
void inspectDevice(WGPUDevice device)
{

    WGPUSupportedFeatures features = WGPU_SUPPORTED_FEATURES_INIT;
    wgpuDeviceGetFeatures(device, &features);
    std::cout << "Device features:" << std::endl;
    std::cout << std::hex;
    for (size_t i = 0; i < features.featureCount; ++i) {
        std::cout << " - 0x" << features.features[i] << std::endl;
    }
    std::cout << std::dec;
    wgpuSupportedFeaturesFreeMembers(features);

    WGPULimits limits = WGPU_LIMITS_INIT;
    bool success = wgpuDeviceGetLimits(device, &limits) == WGPUStatus_Success;

    if (success) {
        std::cout << "Device limits:" << std::endl;
        std::cout << " - maxTextureDimension1D: " << limits.maxTextureDimension1D
                  << std::endl;
        std::cout << " - maxTextureDimension2D: " << limits.maxTextureDimension2D
                  << std::endl;
        std::cout << " - maxTextureDimension3D: " << limits.maxTextureDimension3D
                  << std::endl;
        std::cout << " - maxTextureArrayLayers: " << limits.maxTextureArrayLayers
                  << std::endl;
        std::cout << " - maxBindGroups: " << limits.maxBindGroups << std::endl;
        std::cout << " - maxBindGroupsPlusVertexBuffers: "
                  << limits.maxBindGroupsPlusVertexBuffers << std::endl;
        std::cout << " - maxBindingsPerBindGroup: "
                  << limits.maxBindingsPerBindGroup << std::endl;
        std::cout << " - maxDynamicUniformBuffersPerPipelineLayout: "
                  << limits.maxDynamicUniformBuffersPerPipelineLayout << std::endl;
        std::cout << " - maxDynamicStorageBuffersPerPipelineLayout: "
                  << limits.maxDynamicStorageBuffersPerPipelineLayout << std::endl;
        std::cout << " - maxSampledTexturesPerShaderStage: "
                  << limits.maxSampledTexturesPerShaderStage << std::endl;
        std::cout << " - maxSamplersPerShaderStage: "
                  << limits.maxSamplersPerShaderStage << std::endl;
        std::cout << " - maxStorageBuffersPerShaderStage: "
                  << limits.maxStorageBuffersPerShaderStage << std::endl;
        std::cout << " - maxStorageTexturesPerShaderStage: "
                  << limits.maxStorageTexturesPerShaderStage << std::endl;
        std::cout << " - maxUniformBuffersPerShaderStage: "
                  << limits.maxUniformBuffersPerShaderStage << std::endl;
        std::cout << " - maxUniformBufferBindingSize: "
                  << limits.maxUniformBufferBindingSize << std::endl;
        std::cout << " - maxStorageBufferBindingSize: "
                  << limits.maxStorageBufferBindingSize << std::endl;
        std::cout << " - minUniformBufferOffsetAlignment: "
                  << limits.minUniformBufferOffsetAlignment << std::endl;
        std::cout << " - minStorageBufferOffsetAlignment: "
                  << limits.minStorageBufferOffsetAlignment << std::endl;
        std::cout << " - maxVertexBuffers: " << limits.maxVertexBuffers
                  << std::endl;
        std::cout << " - maxBufferSize: " << limits.maxBufferSize << std::endl;
        std::cout << " - maxVertexAttributes: " << limits.maxVertexAttributes
                  << std::endl;
        std::cout << " - maxVertexBufferArrayStride: "
                  << limits.maxVertexBufferArrayStride << std::endl;
        std::cout << " - maxInterStageShaderVariables: "
                  << limits.maxInterStageShaderVariables << std::endl;
        std::cout << " - maxColorAttachments: " << limits.maxColorAttachments
                  << std::endl;
        std::cout << " - maxColorAttachmentBytesPerSample: "
                  << limits.maxColorAttachmentBytesPerSample << std::endl;
        std::cout << " - maxComputeWorkgroupStorageSize: "
                  << limits.maxComputeWorkgroupStorageSize << std::endl;
        std::cout << " - maxComputeInvocationsPerWorkgroup: "
                  << limits.maxComputeInvocationsPerWorkgroup << std::endl;
        std::cout << " - maxComputeWorkgroupSizeX: "
                  << limits.maxComputeWorkgroupSizeX << std::endl;
        std::cout << " - maxComputeWorkgroupSizeY: "
                  << limits.maxComputeWorkgroupSizeY << std::endl;
        std::cout << " - maxComputeWorkgroupSizeZ: "
                  << limits.maxComputeWorkgroupSizeZ << std::endl;
        std::cout << " - maxComputeWorkgroupsPerDimension: "
                  << limits.maxComputeWorkgroupsPerDimension << std::endl;
    }
}
void fetchBufferDataSync(WGPUInstance instance,
    WGPUBuffer bufferB,
    std::function<void(const void*)> processBufferData)
{
    // We copy here what used to be in main():
    // Context passed to `onBufferBMapped` through theuserdata pointer:
    struct OnBufferBMappedContext {
        bool operationEnded = false; // Turned true as soon as the callback is invoked
        bool mappingIsSuccessful = false; // Turned true only if mapping succeeded
    };

    // This function has the type WGPUBufferMapCallback as defined in webgpu.h
    auto onBufferBMapped = [](WGPUMapAsyncStatus status,
                               struct WGPUStringView message,
                               void* userdata1,
                               void* /* userdata2 */
                               ) static {
        OnBufferBMappedContext& context = *reinterpret_cast<OnBufferBMappedContext*>(userdata1);
        context.operationEnded = true;
        if (status == WGPUMapAsyncStatus_Success) {
            context.mappingIsSuccessful = true;
        } else {
            std::cout << "Could not map buffer B! Status: " << status
                      << ", message: " << toStdStringView(message) << std::endl;
        }
    };

    // We create an instance of the context shared with `onBufferBMapped`
    OnBufferBMappedContext context;

    // And we build the callback info:
    WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = onBufferBMapped;
    callbackInfo.userdata1 = &context;

    // And finally we launch the asynchronous operation
    wgpuBufferMapAsync(bufferB,
        WGPUMapMode_Read,
        0, // offset
        WGPU_WHOLE_MAP_SIZE,
        callbackInfo);

    // Process events until the map operation ended
    wgpuInstanceProcessEvents(instance);
    while (!context.operationEnded) {
        sleepForMilliseconds(200);
        wgpuInstanceProcessEvents(instance);
    }

    if (context.mappingIsSuccessful) {
        const void* bufferData = wgpuBufferGetConstMappedRange(bufferB, 0, WGPU_WHOLE_MAP_SIZE);
        processBufferData(bufferData);
    }
}

wgpu::Surface
glfwCreateWindowWGPUSurface(const wgpu::Instance& instance, GLFWwindow* window)
{
#ifndef __EMSCRIPTEN__
    switch (glfwGetPlatform()) {
#else
    // glfwGetPlatform is not available in older versions of emscripten
    switch (GLFW_PLATFORM_EMSCRIPTEN) {
#endif

#ifdef GLFW_EXPOSE_NATIVE_X11
    case GLFW_PLATFORM_X11: {
        Display* x11_display = glfwGetX11Display();
        Window x11_window = glfwGetX11Window(window);

        WGPUSurfaceSourceXlibWindow fromXlibWindow;
        fromXlibWindow.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
        fromXlibWindow.chain.next = nullptr;
        fromXlibWindow.display = x11_display;
        fromXlibWindow.window = x11_window;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromXlibWindow.chain;
        surfaceDescriptor.label = (WGPUStringView) { nullptr, WGPU_STRLEN };

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#endif // GLFW_EXPOSE_NATIVE_X11

#ifdef GLFW_EXPOSE_NATIVE_WAYLAND
    case GLFW_PLATFORM_WAYLAND: {
        struct wl_display* wayland_display = glfwGetWaylandDisplay();
        struct wl_surface* wayland_surface = glfwGetWaylandWindow(window);

        WGPUSurfaceSourceWaylandSurface fromWaylandSurface;
        fromWaylandSurface.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
        fromWaylandSurface.chain.next = nullptr;
        fromWaylandSurface.display = wayland_display;
        fromWaylandSurface.surface = wayland_surface;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromWaylandSurface.chain;
        surfaceDescriptor.label = (WGPUStringView) { nullptr, WGPU_STRLEN };

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#endif // GLFW_EXPOSE_NATIVE_WAYLAND

#ifdef GLFW_EXPOSE_NATIVE_COCOA
    case GLFW_PLATFORM_COCOA: {
        id metal_layer = [CAMetalLayer layer];
        NSWindow* ns_window = glfwGetCocoaWindow(window);
        [ns_window.contentView setWantsLayer:YES];
        [ns_window.contentView setLayer:metal_layer];

        WGPUSurfaceSourceMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
        fromMetalLayer.chain.next = nullptr;
        fromMetalLayer.layer = metal_layer;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromMetalLayer.chain;
        surfaceDescriptor.label = (WGPUStringView) { nullptr, WGPU_STRLEN };

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#endif // GLFW_EXPOSE_NATIVE_COCOA

#ifdef GLFW_EXPOSE_NATIVE_WIN32
    case GLFW_PLATFORM_WIN32: {
        HWND hwnd = glfwGetWin32Window(window);
        HINSTANCE hinstance = GetModuleHandle(nullptr);

        wgpu::SurfaceSourceWindowsHWND fromWindowsHWND;
        fromWindowsHWND.hinstance = hinstance;
        fromWindowsHWND.hwnd = hwnd;

        wgpu::SurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromWindowsHWND;

        return instance.CreateSurface(&surfaceDescriptor);
    }
#endif // GLFW_EXPOSE_NATIVE_WIN32

#ifdef GLFW_EXPOSE_NATIVE_EMSCRIPTEN
    case GLFW_PLATFORM_EMSCRIPTEN: {
#ifdef WEBGPU_BACKEND_EMDAWNWEBGPU
        WGPUEmscriptenSurfaceSourceCanvasHTMLSelector fromCanvasHTMLSelector;
        fromCanvasHTMLSelector.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
        fromCanvasHTMLSelector.selector = (WGPUStringView) { "canvas", WGPU_STRLEN };
#else
        WGPUSurfaceDescriptorFromCanvasHTMLSelector fromCanvasHTMLSelector;
        fromCanvasHTMLSelector.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        fromCanvasHTMLSelector.selector = "canvas";
#endif
        fromCanvasHTMLSelector.chain.next = nullptr;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromCanvasHTMLSelector.chain;
#ifdef WEBGPU_BACKEND_EMDAWNWEBGPU
        surfaceDescriptor.label = (WGPUStringView) { nullptr, WGPU_STRLEN };
#else
        surfaceDescriptor.label = nullptr;
#endif
        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
    }
#endif // GLFW_EXPOSE_NATIVE_EMSCRIPTEN

    default:
        // Unsupported platform
        return nullptr;
    }
}