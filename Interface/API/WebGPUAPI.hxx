//
// Created by LYS on 12/14/2025.
//

#pragma once

#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#include <functional>
#include <string_view>

#include <GLFW/glfw3.h>

/**
 * Convert a WebGPU string view into a C++ std::string_view.
 */
std::string_view toStdStringView( WGPUStringView wgpuStringView );

/**
 * Convert a C++ std::string_view into a WebGPU string view.
 */
WGPUStringView toWgpuStringView( std::string_view stdStringView );

/**
 * Convert a C string into a WebGPU string view
 */
WGPUStringView toWgpuStringView( const char* cString );

/**
 * Sleep for a given number of milliseconds.
 * This works with both native builds and emscripten, provided that -sASYNCIFY
 * compile option is provided when building with emscripten.
 */
void sleepForMilliseconds( unsigned int milliseconds );

/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapter(options);
 * is roughly equivalent to
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter requestAdapterSync( WGPUInstance instance, WGPURequestAdapterOptions const* options );

/**
 * Utility function to get a WebGPU device, so that
 *     WGPUAdapter device = requestDevice(adapter, options);
 * is roughly equivalent to
 *     const device = await adapter.requestDevice(descriptor);
 * It is very similar to requestAdapter
 */
WGPUDevice requestDeviceSync( WGPUInstance instance, WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor );

/**
 * An example of how we can inspect the capabilities of the hardware through
 * the adapter object.
 */
void inspectAdapter( WGPUAdapter adapter );
/**
 * Display information about a device
 */
void inspectDevice( WGPUDevice device );

/**
 * Fetch data from a GPU buffer back to the CPU.
 * This function blocks until the data is available on CPU, then calls the
 * `processBufferData` callback, and finally unmap the buffer.
 */
void fetchBufferDataSync(
    WGPUInstance                       instance,
    WGPUBuffer                         buffer,
    std::function<void( const void* )> processBufferData );


/*! @brief Creates a WebGPU surface for the specified window.
 *
 *  This function creates a WGPUSurface object for the specified window.
 *
 *  If the surface cannot be created, this function returns `NULL`.
 *
 *  It is the responsibility of the caller to destroy the window surface. The
 *  window surface must be destroyed using `wgpuSurfaceRelease`.
 *
 *  @param[in] instance The WebGPU instance to create the surface in.
 *  @param[in] window The window to create the surface for.
 *  @return The handle of the surface.  This is set to `NULL` if an error
 *  occurred.
 *
 *  @ingroup webgpu
 */
wgpu::Surface glfwCreateWindowWGPUSurface( const wgpu::Instance& instance, GLFWwindow* window );