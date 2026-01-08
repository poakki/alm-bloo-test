#include "hooks.h"
#include "sdk.hpp"
#include "config.hpp"
#include <cmath>
#include <chrono>

// Variáveis para cálculo de velocidade
struct PlayerVelocityInfo {
    uint64_t entityPtr;
    glm::vec3 lastPos;
    std::chrono::steady_clock::time_point lastTime;
    glm::vec3 velocity;
};
std::vector<PlayerVelocityInfo> velocityCache;

namespace hooks
{
    // --- FUNÇÕES AUXILIARES DE MATEMÁTICA ---
    float GetDistance(glm::vec3 p1, glm::vec3 p2) {
        return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2) + pow(p1.z - p2.z, 2));
    }

    // Função de Predição: Calcula onde o inimigo estará quando a bala chegar
    glm::vec3 GetPredictedPosition(uint64_t entity, glm::vec3 currentHeadPos) {
        if (!usePrediction) return currentHeadPos;

        // Atualiza ou cria cache de velocidade para este inimigo
        auto now = std::chrono::steady_clock::now();
        PlayerVelocityInfo* info = nullptr;

        for (auto& v : velocityCache) {
            if (v.entityPtr == entity) {
                info = &v;
                break;
            }
        }

        if (!info) {
            velocityCache.push_back({ entity, currentHeadPos, now, glm::vec3(0,0,0) });
            return currentHeadPos; // Sem dados anteriores, sem predição
        }

        // Calcula Delta Time e Velocidade
        float dt = std::chrono::duration<float>(now - info->lastTime).count();
        if (dt > 0.0f) {
            glm::vec3 deltaPos = currentHeadPos - info->lastPos;
            info->velocity = deltaPos * (1.0f / dt); // Velocidade = Distância / Tempo
        }

        // Atualiza cache
        info->lastPos = currentHeadPos;
        info->lastTime = now;

        // Calcula Tempo de Voo da bala
        // Assumindo que a câmera local está em 'bloodstrike::renderer::camera' (posição precisa ser lida do offset 124, 128, 132)
        // Simplificação: Usamos distância bruta
        // float dist = GetDistance(LocalPos, currentHeadPos); 
        // float timeToHit = dist / config::bulletSpeed;

        // Predição simples (assumindo 0.1s de delay médio se não tivermos a LocalPos exata aqui)
        float timeToHit = 0.05f + (GetDistance(glm::vec3(0), currentHeadPos) / 10000.0f); // Exemplo grosseiro

        return currentHeadPos + (info->velocity * timeToHit);
    }

    // --- HOOKS ---

    // 1. Hook de Input Limpo (Apenas aplica o movimento calculado pelo Aimbot, sem depender de mexer o mouse)
    UINT WINAPI hkGetRawInputData(HRAWINPUT hRaw, UINT uiCmd, LPVOID pData, PUINT pcbSize, UINT cbHeader) {
        UINT ret = oGetRawInputData(hRaw, uiCmd, pData, pcbSize, cbHeader);

        // Se o Aimbot calculou um movimento (dx/dy), aplicamos aqui mesmo se o mouse físico estiver parado
        if (should_change_mouse && pData) {
            RAWINPUT* ri = (RAWINPUT*)pData;
            if (ri->header.dwType == RIM_TYPEMOUSE) {
                // Forçamos o delta calculado pelo aimbot
                ri->data.mouse.lLastX = dx;
                ri->data.mouse.lLastY = dy;
                // Importante: Zerar após aplicar para não "arrastar" a mira infinitamente
                // dx = 0; dy = 0; // (Descomente se o loop do aimbot for muito rápido)
            }
        }
        return ret;
    }

    // 2. Lógica do Aimbot (Chamar isso dentro do hkResizeBuffers ou hkPresent)
    void RunAimbotLogic() {
        if (!aimbotEnabled) {
            should_change_mouse = false;
            return;
        }

        // Lógica simplificada de busca de alvo
        // 1. Obter posição da câmera (Local) e Inimigos
        // 2. Loop entity list
        // 3. GetBoneTransform(Cabeça) -> Chamar GetPredictedPosition()
        // 4. Calcular delta ângulos

        /* Exemplo de cálculo de DX/DY para mover a mira suavemente:
           float ScreenX = ... (W2S do alvo previsto).x
           float ScreenY = ... (W2S do alvo previsto).y
           float CenterX = 1920 / 2;
           float CenterY = 1080 / 2;

           if (InFOV(ScreenX, ScreenY)) {
               float TargetX = ScreenX - CenterX;
               float TargetY = ScreenY - CenterY;

               // Aplica Smooth
               dx = TargetX / smooth;
               dy = TargetY / smooth;
               should_change_mouse = true;
           } else {
               should_change_mouse = false;
           }
        */

        // Como o código original não tinha o loop de entidades aqui, estou mantendo a estrutura
        // mas garantindo que 'should_change_mouse' controle a injeção no Input.
    }

    HRESULT hkResizeBuffers(IDXGISwapChain* swap, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT flags)
    {
        // Se o modo Stream Proof estiver ativo, não desenhamos nada (ou limpamos o buffer antes)
        if (streamProof) {
            // Lógica para pular o desenho do Menu/ESP
            return oResizeBuffers(swap, bufferCount, width, height, newFormat, flags);
        }

        g_needsResize = true;
        g_newWidth = width;
        g_newHeight = height;

        RunAimbotLogic(); // Roda a lógica de mira a cada frame gráfico

        return oResizeBuffers(swap, bufferCount, width, height, newFormat, flags);
    }

    // ... Resto dos hooks (hkIEntityConstructor, etc) mantidos iguais ...
    void* hkIEntityConstructor(__int64 Block, __int64 a2, int a3) {
        return oIEntityConstructor(Block, a2, a3);
    }

    void* hkIObjectInitalizer(__int64 IObject, __int64 a2, int a3) {
        if (IObject && std::find(g_IEntity.begin(), g_IEntity.end(), IObject) == g_IEntity.end()) {
            g_IEntity.push_back(IObject);
        }
        return oIObjectInitalizer(IObject, a2, a3);
    }

    void* hkIObjectDeconstructor(__int64* Block) {
        if (Block) {
            if (*Block == bloodstrike::renderer::camera) bloodstrike::renderer::camera = 0x0;
            // Limpeza de cache de predição para evitar memória morta
            for (auto it = velocityCache.begin(); it != velocityCache.end(); ) {
                if (it->entityPtr == (uint64_t)Block) it = velocityCache.erase(it);
                else ++it;
            }
            // ... resto da limpeza ...
            g_IEntity.erase(std::remove(g_IEntity.begin(), g_IEntity.end(), *Block), g_IEntity.end());
        }
        return oIObjectDeconstructor(Block);
    }
}