#include "Components.h"

#include <iostream>

#ifdef DIRECTX_RENDERER
#include "../renderer/DirectXRenderer.h"

namespace WorldEditor {

// Static renderer reference for safe resource cleanup
DirectXRenderer* MeshComponent::s_renderer = nullptr;

MeshComponent::~MeshComponent() {
    // Safely release DirectX resources using deferred deletion
    if (s_renderer) {
        try {
            // Проверяем что device еще доступен перед освобождением ресурсов
            auto* device = s_renderer->GetDevice();
            if (device) {
                // Проверяем состояние device
                HRESULT deviceRemovedReason = device->GetDeviceRemovedReason();
                if (SUCCEEDED(deviceRemovedReason)) {
                    // Device в порядке - можем безопасно освобождать ресурсы
                    if (vertexBuffer) {
                        s_renderer->DeferredReleaseResource(vertexBuffer);
                        vertexBuffer.Reset();
                    }
                    if (indexBuffer) {
                        s_renderer->DeferredReleaseResource(indexBuffer);
                        indexBuffer.Reset();
                    }
                    if (vertexBufferUpload) {
                        s_renderer->DeferredReleaseResource(vertexBufferUpload);
                        vertexBufferUpload.Reset();
                    }
                    if (indexBufferUpload) {
                        s_renderer->DeferredReleaseResource(indexBufferUpload);
                        indexBufferUpload.Reset();
                    }
                    if (perObjectConstantBuffer) {
                        s_renderer->DeferredReleaseResource(perObjectConstantBuffer);
                        perObjectConstantBuffer.Reset();
                    }
                    if (perObjectConstantBufferUpload) {
                        s_renderer->DeferredReleaseResource(perObjectConstantBufferUpload);
                        perObjectConstantBufferUpload.Reset();
                    }
                } else {
                    // Device removed - просто обнуляем указатели без освобождения
                    std::cerr << "Device removed in MeshComponent destructor, reason: 0x" << std::hex << deviceRemovedReason << std::endl;
                    vertexBuffer.Reset();
                    indexBuffer.Reset();
                    vertexBufferUpload.Reset();
                    indexBufferUpload.Reset();
                    perObjectConstantBuffer.Reset();
                    perObjectConstantBufferUpload.Reset();
                }
            } else {
                // Device недоступен - просто обнуляем указатели без освобождения
                vertexBuffer.Reset();
                indexBuffer.Reset();
                vertexBufferUpload.Reset();
                indexBufferUpload.Reset();
                perObjectConstantBuffer.Reset();
                perObjectConstantBufferUpload.Reset();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error in MeshComponent destructor: " << e.what() << std::endl;
            // В случае ошибки просто обнуляем указатели
            vertexBuffer.Reset();
            indexBuffer.Reset();
            vertexBufferUpload.Reset();
            indexBufferUpload.Reset();
            perObjectConstantBuffer.Reset();
            perObjectConstantBufferUpload.Reset();
        }
        catch (...) {
            std::cerr << "Unknown error in MeshComponent destructor" << std::endl;
            // В случае любой ошибки просто обнуляем указатели
            vertexBuffer.Reset();
            indexBuffer.Reset();
            vertexBufferUpload.Reset();
            indexBufferUpload.Reset();
            perObjectConstantBuffer.Reset();
            perObjectConstantBufferUpload.Reset();
        }
    } else {
        // Renderer недоступен - просто обнуляем указатели
        vertexBuffer.Reset();
        indexBuffer.Reset();
        vertexBufferUpload.Reset();
        indexBufferUpload.Reset();
        perObjectConstantBuffer.Reset();
        perObjectConstantBufferUpload.Reset();
    }
}

} // namespace WorldEditor

#endif // DIRECTX_RENDERER