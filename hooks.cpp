#include "hooks.h"
#include "sdk.hpp" // Para IsValidPtr

std::vector<uint64_t> g_IEntity;
std::mutex entityMutex;

// Configs Padrão
bool aimbotMaster = true;
float aimFov = 150.0f;
float aimSpeed = 0.35f;
bool aimRandomize = true;
float aimHeadChance = 0.5f;
bool aimAtBody = false;

bool espBox = true;
bool espSkeleton = true;
bool espLines = true;
bool espDistance = true;
bool showCrosshair = false;

float colorBox[3] = { 1.0f, 0.0f, 0.0f };
float colorSkeleton[3] = { 1.0f, 1.0f, 1.0f };
float colorLines[3] = { 0.0f, 1.0f, 0.0f };

// Ponteiros
tIObjectInitalizer oIObjectInitalizer = nullptr;
tIObjectDeconstructor oIObjectDeconstructor = nullptr;
tPresent oPresent = nullptr;
tResizeBuffers oResizeBuffers = nullptr;
tGetBoneTransform oGetBoneTransform = nullptr;
tGetRawInputData oGetRawInputData = nullptr;

namespace bloodstrike {
    namespace renderer {
        ID3D11Device* deviceInstance = nullptr;
        ID3D11DeviceContext* contextInstance = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        HWND hWindow = NULL;
        bool hooked = false;
        uint64_t camera = 0;
        uint64_t hwnd = 0;
    }
    namespace funcs {
        uint64_t Messiah__IObject__Initalizer = 0x14109E010;
        uint64_t Messiah__IObject__Deconstructor = 0x14109E050;
        uint64_t Messiah__GetBoneTransform = 0x1410D3900;
    }
}

namespace hooks {
    void* hkIObjectInitalizer(__int64 IObject, __int64 a2, int a3) {
        std::lock_guard<std::mutex> lock(entityMutex);
        if (sdk::IsValidPtr((void*)IObject)) {
            bool found = false;
            for (auto e : g_IEntity) if (e == (uint64_t)IObject) found = true;
            if (!found) g_IEntity.push_back((uint64_t)IObject);
        }
        return oIObjectInitalizer(IObject, a2, a3);
    }

    void* hkIObjectDeconstructor(__int64* Block) {
        if (sdk::IsValidPtr(Block)) {
            if ((uint64_t)*Block == bloodstrike::renderer::camera)
                bloodstrike::renderer::camera = 0;

            std::lock_guard<std::mutex> lock(entityMutex);
            for (size_t i = 0; i < g_IEntity.size(); ++i) {
                if (g_IEntity[i] == (uint64_t)*Block) {
                    g_IEntity.erase(g_IEntity.begin() + i);
                    break;
                }
            }
        }
        return oIObjectDeconstructor(Block);
    }

    __int64 hkGetBoneTransform(__int64 a1, __int64 a2, __int64 a3) {
        return oGetBoneTransform(a1, a2, a3);
    }

    UINT WINAPI hkGetRawInputData(HRAWINPUT hRaw, UINT uiCmd, LPVOID pData, PUINT pcbSize, UINT cbHeader) {
        return oGetRawInputData(hRaw, uiCmd, pData, pcbSize, cbHeader);
    }

    HRESULT hkResizeBuffers(IDXGISwapChain* swap, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT flags) {
        if (bloodstrike::renderer::rtv) {
            bloodstrike::renderer::contextInstance->OMSetRenderTargets(0, nullptr, nullptr);
            bloodstrike::renderer::rtv->Release();
            bloodstrike::renderer::rtv = nullptr;
        }
        HRESULT hr = oResizeBuffers(swap, bufferCount, width, height, newFormat, flags);
        if (SUCCEEDED(hr) && ImGui::GetCurrentContext()) {
            ImGui::GetIO().DisplaySize = ImVec2((float)width, (float)height);
        }
        return hr;
    }
}