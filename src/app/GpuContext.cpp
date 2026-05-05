// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/app/GpuContext.cpp
//
//  wgpu-native adapter + device initialisation (wgpu-native v29+).
//
//  The webgpu.h v29 callback API uses WGPURequest*CallbackInfo structs instead
//  of bare function pointers. wgpu-native still resolves these synchronously on
//  the calling thread in native (non-browser) builds, so WGPUCallbackMode_
//  AllowSpontaneous preserves the old "fires before function returns" behaviour.
// ─────────────────────────────────────────────────────────────────────────────

#include "GpuContext.h"
#include <cstdio>
#include <stdexcept>

namespace dexilate {

namespace {

WGPUAdapter requestAdapter(WGPUInstance instance, WGPUSurface surface) {
    WGPURequestAdapterOptions opts = {};
    opts.compatibleSurface = surface;
    opts.powerPreference   = WGPUPowerPreference_HighPerformance;

    WGPUAdapter result = nullptr;
    WGPURequestAdapterCallbackInfo cbInfo = {};
    cbInfo.mode     = WGPUCallbackMode_AllowSpontaneous;
    cbInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                         WGPUStringView /*msg*/, void* ud1, void* /*ud2*/) {
        if (status == WGPURequestAdapterStatus_Success)
            *static_cast<WGPUAdapter*>(ud1) = adapter;
    };
    cbInfo.userdata1 = &result;
    wgpuInstanceRequestAdapter(instance, &opts, cbInfo);
    return result;
}

WGPUDevice requestDevice(WGPUAdapter adapter) {
    WGPUQueueDescriptor queueDesc = {};
    queueDesc.label.data   = "Dexilate Queue";
    queueDesc.label.length = WGPU_STRLEN;

    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.label.data   = "Dexilate Device";
    deviceDesc.label.length = WGPU_STRLEN;
    deviceDesc.defaultQueue = queueDesc;

    deviceDesc.deviceLostCallbackInfo.mode     = WGPUCallbackMode_AllowSpontaneous;
    deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const* /*dev*/,
        WGPUDeviceLostReason reason, WGPUStringView msg,
        void* /*ud1*/, void* /*ud2*/) {
#ifndef NDEBUG
        (void)reason;
        if (msg.data) fprintf(stderr, "[wgpu] device lost: %.*s\n",
                              (int)msg.length, msg.data);
#else
        (void)reason; (void)msg;
#endif
    };

    WGPUDevice result = nullptr;
    WGPURequestDeviceCallbackInfo cbInfo = {};
    cbInfo.mode     = WGPUCallbackMode_AllowSpontaneous;
    cbInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                         WGPUStringView /*msg*/, void* ud1, void* /*ud2*/) {
        if (status == WGPURequestDeviceStatus_Success)
            *static_cast<WGPUDevice*>(ud1) = device;
    };
    cbInfo.userdata1 = &result;
    wgpuAdapterRequestDevice(adapter, &deviceDesc, cbInfo);
    return result;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
GpuContext GpuContext::create(WGPUInstance instance, WGPUSurface surface) {
    GpuContext ctx;
    ctx._adapter = requestAdapter(instance, surface);
    if (!ctx._adapter)
        throw std::runtime_error("GpuContext: no compatible GPU adapter found");

    ctx._device = requestDevice(ctx._adapter);
    if (!ctx._device)
        throw std::runtime_error("GpuContext: device creation failed");

    ctx._queue = wgpuDeviceGetQueue(ctx._device);
    return ctx;
}

GpuContext::~GpuContext() {
    if (_queue)   wgpuQueueRelease(_queue);
    if (_device)  wgpuDeviceRelease(_device);
    if (_adapter) wgpuAdapterRelease(_adapter);
}

GpuContext::GpuContext(GpuContext&& o) noexcept
    : _adapter(o._adapter), _device(o._device), _queue(o._queue)
{
    o._adapter = nullptr;
    o._device  = nullptr;
    o._queue   = nullptr;
}

GpuContext& GpuContext::operator=(GpuContext&& o) noexcept {
    if (this != &o) {
        if (_queue)   wgpuQueueRelease(_queue);
        if (_device)  wgpuDeviceRelease(_device);
        if (_adapter) wgpuAdapterRelease(_adapter);
        _adapter = o._adapter; _device = o._device; _queue = o._queue;
        o._adapter = nullptr;  o._device = nullptr; o._queue = nullptr;
    }
    return *this;
}

} // namespace dexilate
