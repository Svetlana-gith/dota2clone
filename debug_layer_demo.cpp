// Демонстрация работы DX12 Debug Layer
#include <iostream>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

void DemonstrateDebugLayer() {
    std::cout << "\n=== ДЕМОНСТРАЦИЯ DX12 DEBUG LAYER ===" << std::endl;
    
    // 1. Попытка включить Debug Layer
    std::cout << "\n1. Попытка включения Debug Layer..." << std::endl;
    
#ifdef DX12_ENABLE_DEBUG_LAYER
    ComPtr<ID3D12Debug> debugController;
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    
    if (SUCCEEDED(hr)) {
        debugController->EnableDebugLayer();
        std::cout << "✅ DX12 Debug Layer ВКЛЮЧЕН!" << std::endl;
        
        // Включаем GPU-based validation
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController.As(&debugController1))) {
            debugController1->SetEnableGPUBasedValidation(TRUE);
            std::cout << "✅ GPU-based validation ВКЛЮЧЕНА!" << std::endl;
        }
    } else {
        std::cout << "❌ Debug Layer недоступен (Graphics Tools не установлены)" << std::endl;
        std::cout << "   HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
    }
#else
    std::cout << "❌ Debug Layer ОТКЛЮЧЕН в сборке (DX12_ENABLE_DEBUG_LAYER не определен)" << std::endl;
#endif

    // 2. Создание устройства
    std::cout << "\n2. Создание D3D12 устройства..." << std::endl;
    
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        std::cout << "❌ Не удалось создать DXGI Factory" << std::endl;
        return;
    }
    
    ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(factory->EnumAdapters1(0, &adapter))) {
        std::cout << "❌ Не удалось найти адаптер" << std::endl;
        return;
    }
    
    ComPtr<ID3D12Device> device;
    HRESULT deviceHr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    
    if (SUCCEEDED(deviceHr)) {
        std::cout << "✅ D3D12 устройство создано успешно!" << std::endl;
        
        // 3. Настройка Info Queue (если Debug Layer активен)
#ifdef DX12_ENABLE_DEBUG_LAYER
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(device.As(&infoQueue))) {
            std::cout << "\n3. Настройка Info Queue..." << std::endl;
            
            // Показываем разные режимы
            std::cout << "   Режимы обработки ошибок:" << std::endl;
            std::cout << "   - CORRUPTION: прерывание выполнения ✅" << std::endl;
            std::cout << "   - ERROR: только логирование 📝" << std::endl;
            std::cout << "   - WARNING: только логирование 📝" << std::endl;
            
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
            
            std::cout << "✅ Info Queue настроена в 'умном' режиме!" << std::endl;
        }
#endif
        
        // 4. Демонстрация работы
        std::cout << "\n4. Что происходит при ошибках:" << std::endl;
        std::cout << "   БЕЗ Debug Layer:" << std::endl;
        std::cout << "   - Ошибки игнорируются 🤐" << std::endl;
        std::cout << "   - Приложение может работать некорректно" << std::endl;
        std::cout << "   - Сложно найти баги" << std::endl;
        
        std::cout << "\n   С АГРЕССИВНЫМ Debug Layer (старый код):" << std::endl;
        std::cout << "   - Любая ошибка → CRASH 💥" << std::endl;
        std::cout << "   - SetBreakOnSeverity(ERROR, TRUE)" << std::endl;
        std::cout << "   - Приложение падает на проблемных ПК" << std::endl;
        
        std::cout << "\n   С УМНЫМ Debug Layer (новый код):" << std::endl;
        std::cout << "   - Критические ошибки → CRASH 💥" << std::endl;
        std::cout << "   - Обычные ошибки → логирование 📝" << std::endl;
        std::cout << "   - Предупреждения → логирование 📝" << std::endl;
        std::cout << "   - Приложение стабильно + помогает в разработке ✅" << std::endl;
        
    } else {
        std::cout << "❌ Не удалось создать D3D12 устройство" << std::endl;
        std::cout << "   HRESULT: 0x" << std::hex << deviceHr << std::dec << std::endl;
    }
    
    std::cout << "\n=== ДЕМОНСТРАЦИЯ ЗАВЕРШЕНА ===" << std::endl;
}

int main() {
    DemonstrateDebugLayer();
    
    std::cout << "\nНажмите Enter для выхода..." << std::endl;
    std::cin.get();
    
    return 0;
}