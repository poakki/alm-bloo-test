#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <d3d11.h>

// GLM (Baseado nos arquivos que você enviou)
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// ImGui
#include "imgui.h"
#include "imgui_internal.h"

namespace sdk {
    inline bool IsValidPtr(void* p) {
        return p != nullptr && !IsBadReadPtr(p, 8);
    }

    // World To Screen Original (Usando GLM)
    inline bool w2s(uint64_t cameraAddr, glm::vec3 worldPos, glm::vec2& screenPos) {
        if (!cameraAddr || IsBadReadPtr((void*)(cameraAddr + 0x58), 64)) return false;

        // Lê a matriz como glm::mat4
        glm::mat4 viewProj = *(glm::mat4*)(cameraAddr + 0x58);

        // Acesso direto à memória da matriz (Row Major layout comum)
        float* m = (float*)&viewProj;

        // Cálculo de projeção
        float w = m[3] * worldPos.x + m[7] * worldPos.y + m[11] * worldPos.z + m[15];

        if (w < 0.001f) return false;

        float x = m[0] * worldPos.x + m[4] * worldPos.y + m[8] * worldPos.z + m[12];
        float y = m[1] * worldPos.x + m[5] * worldPos.y + m[9] * worldPos.z + m[13];

        ImGuiIO& io = ImGui::GetIO();
        screenPos.x = (io.DisplaySize.x / 2.0f) * (1.0f + x / w);
        screenPos.y = (io.DisplaySize.y / 2.0f) * (1.0f - y / w);

        return true;
    }

    // Soma de Matrizes para Ossos (Ajustado para GLM)
    inline void MessiahMatrixAdd(glm::mat4x3 boneMatrix, glm::mat4x3 transformMatrix, glm::vec3& out) {
        // Acessa as colunas de posição (índice 3 em uma matriz 4x3 row-major ou similar)
        // Ajuste para pegar a translação (última coluna/linha)
        float* b = (float*)&boneMatrix;
        float* t = (float*)&transformMatrix;

        // Soma simples das posições (x, y, z)
        // Assumindo layout onde posição está nos últimos 3 floats de cada linha ou na última coluna
        // Ajuste padrão:
        out.x = b[9] + t[9];
        out.y = b[10] + t[10];
        out.z = b[11] + t[11];

        // Se o bone ficar torto, tente:
        // out.x = boneMatrix[3][0] + transformMatrix[3][0]; ... etc (se for column major)
    }
}

namespace visuals {
    // Helper para converter ImU32 para array float (se necessário)
    inline float* U32ToArray(ImU32 color) {
        static float arr[4];
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
        arr[0] = c.x; arr[1] = c.y; arr[2] = c.z; arr[3] = c.w;
        return arr;
    }

    inline void DrawCornerBox(int x, int y, int w, int h, int thickness, ImColor color) {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        float lw = w / 4.0f;
        float lh = h / 4.0f;

        // Top Left
        draw->AddLine(ImVec2((float)x, (float)y), ImVec2((float)x + lw, (float)y), color, (float)thickness);
        draw->AddLine(ImVec2((float)x, (float)y), ImVec2((float)x, (float)y + lh), color, (float)thickness);

        // Top Right
        draw->AddLine(ImVec2((float)(x + w), (float)y), ImVec2((float)(x + w) - lw, (float)y), color, (float)thickness);
        draw->AddLine(ImVec2((float)(x + w), (float)y), ImVec2((float)(x + w), (float)y + lh), color, (float)thickness);

        // Bottom Left
        draw->AddLine(ImVec2((float)x, (float)(y + h)), ImVec2((float)x + lw, (float)(y + h)), color, (float)thickness);
        draw->AddLine(ImVec2((float)x, (float)(y + h)), ImVec2((float)x, (float)(y + h) - lh), color, (float)thickness);

        // Bottom Right
        draw->AddLine(ImVec2((float)(x + w), (float)(y + h)), ImVec2((float)(x + w) - lw, (float)(y + h)), color, (float)thickness);
        draw->AddLine(ImVec2((float)(x + w), (float)(y + h)), ImVec2((float)(x + w), (float)(y + h) - lh), color, (float)thickness);
    }

    inline void DrawLabel(std::string text, glm::vec2 pos, ImColor color, bool centered = true) {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        float px = pos.x;
        float py = pos.y;

        if (centered) px -= textSize.x / 2.0f;

        draw->AddText(ImVec2(px + 1, py + 1), IM_COL32(0, 0, 0, 255), text.c_str());
        draw->AddText(ImVec2(px, py), color, text.c_str());
    }
}