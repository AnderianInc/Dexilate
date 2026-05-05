// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/app/main.cpp
//
//  Phase 0 — Hello Dexilate entry point.
//
//  Creates a platform window, initialises wgpu-native, and runs a render loop
//  that clears the canvas to #F0F0F0 (canvas gray) at 60fps until the user
//  closes the window.  No shaders or draw calls are needed for a clear pass.
// ─────────────────────────────────────────────────────────────────────────────

#include "dexilate/platform/Platform.h"
#include "dexilate/platform/IWindow.h"
#include "GpuContext.h"

#include <webgpu/wgpu.h>
#include <cstdio>
#include <stdexcept>

// Platform-specific surface descriptor headers
#ifdef __APPLE__
#  include <QuartzCore/CAMetalLayer.h>
#elif defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(DEXILATE_WAYLAND)
#  include <wayland-client.h>
#else
#  include <X11/Xlib.h>
#endif

using namespace dexilate::platform;

namespace {

WGPUSurface createSurface(WGPUInstance instance, const IWindow& win) {
    auto handle = win.gpuSurfaceHandle();

#ifdef __APPLE__
    WGPUSurfaceSourceMetalLayer metalDesc = {};
    metalDesc.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    metalDesc.layer       = handle.nativeWindowHandle;  // CAMetalLayer*

    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&metalDesc);
    return wgpuInstanceCreateSurface(instance, &surfDesc);

#elif defined(_WIN32)
    WGPUSurfaceSourceWindowsHWND hwndDesc = {};
    hwndDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    hwndDesc.hwnd        = handle.nativeWindowHandle;
    hwndDesc.hinstance   = GetModuleHandleW(nullptr);

    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&hwndDesc);
    return wgpuInstanceCreateSurface(instance, &surfDesc);

#elif defined(DEXILATE_WAYLAND)
    if (handle.nativeDisplayHandle) {
        // Runtime selected Wayland backend (WAYLAND_DISPLAY is set)
        WGPUSurfaceSourceWaylandSurface wlDesc = {};
        wlDesc.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
        wlDesc.surface     = handle.nativeWindowHandle;   // wl_surface*
        wlDesc.display     = handle.nativeDisplayHandle;  // wl_display*

        WGPUSurfaceDescriptor surfDesc = {};
        surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wlDesc);
        return wgpuInstanceCreateSurface(instance, &surfDesc);
    }
    // Fall through: WaylandWindow fell back to X11 at creation time
    {
        WGPUSurfaceSourceXlibWindow xlibDesc = {};
        xlibDesc.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
        xlibDesc.display     = handle.nativeDisplayHandle;
        xlibDesc.window      = reinterpret_cast<uint64_t>(handle.nativeWindowHandle);

        WGPUSurfaceDescriptor surfDesc = {};
        surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&xlibDesc);
        return wgpuInstanceCreateSurface(instance, &surfDesc);
    }

#else  // Linux X11 only (no Wayland build)
    WGPUSurfaceSourceXlibWindow xlibDesc = {};
    xlibDesc.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    xlibDesc.display     = handle.nativeDisplayHandle;   // Display*
    xlibDesc.window      = reinterpret_cast<uint64_t>(handle.nativeWindowHandle);  // Window (XID)

    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&xlibDesc);
    return wgpuInstanceCreateSurface(instance, &surfDesc);
#endif
}

WGPUTextureFormat configureSurface(WGPUSurface surface, WGPUAdapter adapter,
                                   WGPUDevice device,
                                   uint32_t width, uint32_t height) {
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(surface, adapter, &caps);

    WGPUTextureFormat fmt = (caps.formatCount > 0)
        ? caps.formats[0]
        : WGPUTextureFormat_BGRA8Unorm;

    WGPUSurfaceConfiguration cfg = {};
    cfg.device           = device;
    cfg.format           = fmt;
    cfg.width            = width;
    cfg.height           = height;
    cfg.presentMode      = WGPUPresentMode_Fifo;
    cfg.alphaMode        = WGPUCompositeAlphaMode_Auto;
    cfg.usage            = WGPUTextureUsage_RenderAttachment;
    wgpuSurfaceConfigure(surface, &cfg);

    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return fmt;
}

void renderFrame(WGPUSurface surface, WGPUDevice device, WGPUQueue queue) {
    WGPUSurfaceTexture st = {};
    wgpuSurfaceGetCurrentTexture(surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        if (st.texture) wgpuTextureRelease(st.texture);
        return;
    }

    WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);

    // #F0F0F0 canvas gray — (240/255) ≈ 0.9412
    WGPURenderPassColorAttachment attachment = {};
    attachment.view       = view;
    attachment.loadOp     = WGPULoadOp_Clear;
    attachment.storeOp    = WGPUStoreOp_Store;
    attachment.clearValue = { 0.9412f, 0.9412f, 0.9412f, 1.0f };
    attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;  // required in wgpu 0.20

    WGPURenderPassDescriptor passDesc = {};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments     = &attachment;

    WGPUCommandEncoderDescriptor encDesc = {};
    encDesc.label.data   = "Dexilate Frame";
    encDesc.label.length = WGPU_STRLEN;
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, &encDesc);

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &passDesc);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuCommandEncoderRelease(enc);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    wgpuTextureViewRelease(view);
    wgpuTextureRelease(st.texture);

    wgpuSurfacePresent(surface);
}

} // namespace

int main() {
    Platform::init();

    WindowDesc desc;
    desc.title  = "Dexilate";
    desc.width  = 1280;
    desc.height = 800;

    std::unique_ptr<IWindow> window;
    try {
        window = IWindow::create(desc);
    } catch (const std::exception& e) {
        fprintf(stderr, "[dexilate] window creation failed: %s\n", e.what());
        Platform::shutdown();
        return 1;
    }
    window->show();

    // ── wgpu instance ──────────────────────────────────────────────────────────
    WGPUInstanceDescriptor instDesc = {};
    WGPUInstance instance = wgpuCreateInstance(&instDesc);
    if (!instance) {
        fprintf(stderr, "[dexilate] wgpuCreateInstance failed\n");
        Platform::shutdown();
        return 1;
    }

    // ── platform surface ───────────────────────────────────────────────────────
    WGPUSurface surface = createSurface(instance, *window);
    if (!surface) {
        fprintf(stderr, "[dexilate] surface creation failed\n");
        wgpuInstanceRelease(instance);
        Platform::shutdown();
        return 1;
    }

    // ── adapter + device ───────────────────────────────────────────────────────
    dexilate::GpuContext gpu = dexilate::GpuContext::create(instance, surface);

    // ── initial surface configuration ──────────────────────────────────────────
    configureSurface(surface, gpu.adapter(), gpu.device(),
                     window->physicalWidth(), window->physicalHeight());

    // Reconfigure when the window is resized
    bool     needsConfigure = false;
    uint32_t resizeW = 0, resizeH = 0;
    window->setResizeCallback([&](uint32_t w, uint32_t h) {
        resizeW = w; resizeH = h;
        needsConfigure = true;
    });

    // ── render loop ────────────────────────────────────────────────────────────
    while (!window->shouldClose()) {
        window->pollEvents();

        if (needsConfigure && resizeW > 0 && resizeH > 0) {
            needsConfigure = false;
            configureSurface(surface, gpu.adapter(), gpu.device(), resizeW, resizeH);
        }

        renderFrame(surface, gpu.device(), gpu.queue());
    }

    // ── cleanup ────────────────────────────────────────────────────────────────
    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
    // gpu destructor releases adapter / device / queue

    Platform::shutdown();
    return 0;
}
