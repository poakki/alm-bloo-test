#include "pch.h"
#include "hooks.h" 
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>

// Headers do ImGui
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

using namespace sdk;

// [CRÍTICO] Definição única da variável global
bool menuOpen = false;
WNDPROC oWndProc;

// Forward Declaration
HRESULT hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

// Configurações
struct ConfigSettings {
    bool masterAimbot = true;
    bool aimbotDrawFov = true;
    bool aimbotSnaplines = false;
    ImVec4 aimbotFovColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    int aimLocation = 0;
    float aimbotHeadChance = 0.5f;
    float aimbotFov = 150.0f;
    float aimbotSmooth = 5.0f;
    float aimbotMaxDist = 100.0f;

    bool masterEsp = true;
    bool espBox = true;
    ImVec4 espBoxColor = ImVec4(1.0f, 0.0f, 0.3f, 1.0f);
    bool espSkeleton = true;
    ImVec4 espSkeletonColor = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
    bool espDistance = true;

    int currentThemeIdx = 0;
    float animSpeed = 0.05f;
} cfg;

int currentTab = 0;
float menuAlpha = 0.0f;
bool initHookInput = false;

void SetupStyles();
void RenderConfigTab();

LRESULT __stdcall hkWndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (menuOpen) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return true;
    }
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

void CheatTick()
{
    bool debugMode = false;
#ifdef aimbotDebug
    debugMode = aimbotDebug;
#endif

    if (!cfg.masterEsp && !cfg.masterAimbot && !debugMode) return;

    uint64_t ClientEngine = *(uint64_t*)(base + bloodstrike::offsets::Messiah__ClientEngine);
    if (!ClientEngine) return;

    uint64_t IGameplay = *(uint64_t*)(ClientEngine + 0x58);
    if (!IGameplay) return;

    uint64_t ClientPlayer = *(uint64_t*)(IGameplay + 0x58);
    if (!ClientPlayer) return;

    bloodstrike::renderer::camera = *(uint64_t*)(ClientPlayer + 0x238);
    bloodstrike::renderer::localActor = *(uint64_t*)(ClientPlayer + 0x288);

    if (!bloodstrike::renderer::camera || !bloodstrike::renderer::localActor) return;
    glm::mat4x3 local_trans = *(glm::mat4x3*)(bloodstrike::renderer::localActor + 0x58);
    glm::vec3 local_pos = local_trans[3];

    float closest_dst = FLT_MAX;
    glm::vec2 target_pos{ -1.0f, -1.0f };

    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 sc = { io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f };

    uint64_t entityListStart = *(uint64_t*)(base + bloodstrike::offsets::Messiah__EntityList);
    if (!entityListStart) return;

    uint64_t head = *(uint64_t*)(entityListStart + 0x8);
    if (!head) return;

    uint64_t currentActor = *(uint64_t*)(head);
    bool isMisc = false;

    if (currentActor)
    {
        do
        {
            isMisc = false;
            hooks::targetAddr = 0x0;
            uint64_t actorInstance = *(uint64_t*)(currentActor + 0x18);
            if (!actorInstance) { currentActor = *(uint64_t*)(currentActor); continue; }
            uint64_t actorProps = *(uint64_t*)(actorInstance + 0x278);
            if (!actorProps) { currentActor = *(uint64_t*)(currentActor); continue; }
            uint64_t actorComponent = *(uint64_t*)(actorProps + 0x18);
            if (!actorComponent) { currentActor = *(uint64_t*)(currentActor); continue; }
            uint64_t IEntity = *(uint64_t*)(actorComponent + 0x40);
            if (!IEntity) { currentActor = *(uint64_t*)(currentActor); continue; }
            uint64_t entityMask = *(uint64_t*)(IEntity + 0x2e0);
            if (entityMask != 2) { isMisc = true; }

            if (IEntity == bloodstrike::renderer::localActor)
            {
                currentActor = *(uint64_t*)(currentActor);
                continue;
            }

            uint64_t IArea = *(uint64_t*)(IEntity + 0x88);
            if (IArea == 0x0) { currentActor = *(uint64_t*)(currentActor); continue; }
            uint64_t pose = *(uint64_t*)(actorInstance + 0x18);
            if (!pose) { currentActor = *(uint64_t*)(currentActor); continue; }
            uint64_t BipedPose = *(uint64_t*)(pose + 0x90);
            if (!BipedPose) { currentActor = *(uint64_t*)(currentActor); continue; }

            BipedPose += 0x8;

            glm::vec2 result;
            glm::mat4x3 trans = *(glm::mat4x3*)(IEntity + 0x58);
            XMFLOAT3X4 dxTrans = *(XMFLOAT3X4*)(IEntity + 0x58);

            glm::vec3 coords = trans[3];
            float d = (float)glm::distance(local_pos, coords);
            float d2 = std::floor(d);

            if (w2s(bloodstrike::renderer::camera, coords, result))
            {
                if ((result.x > io.DisplaySize.x || result.y > io.DisplaySize.y) || (result.x < 1.0f || result.y < 1.0f))
                {
                    currentActor = *(uint64_t*)(currentActor); continue;
                }

                int dst_m = static_cast<int>(d2 / 5.0f);

                if (dst_m < 1000 && !isMisc)
                {
                    if (cfg.masterAimbot) {
                        if ((float)dst_m <= cfg.aimbotMaxDist)
                        {
                            glm::vec2 neck, spine3; glm::vec3 _neck, _spine3;
                            uint64_t boneStart = *(uint64_t*)((7 * 0x8) + BipedPose);
                            if (boneStart) { MessiahMatrixAdd(*(XMFLOAT3X4*)(boneStart + 0x30), dxTrans, _neck); w2s(bloodstrike::renderer::camera, _neck, neck); }
                            boneStart = *(uint64_t*)((4 * 0x8) + BipedPose);
                            if (boneStart) { MessiahMatrixAdd(*(XMFLOAT3X4*)(boneStart + 0x30), dxTrans, _spine3); w2s(bloodstrike::renderer::camera, _spine3, spine3); }

                            float dist = (float)glm::distance(sc, neck);
                            if (dist < closest_dst)
                            {
                                hooks::targetAddr = IEntity;
                                closest_dst = dist;

                                bool aimHead = false;
                                if (cfg.aimLocation == 0) aimHead = true;
                                else if (cfg.aimLocation == 1) aimHead = false;
                                else {
                                    float r = static_cast <float>(rand()) / static_cast <float>(RAND_MAX);
                                    aimHead = (r < cfg.aimbotHeadChance);
                                }

                                target_pos = aimHead ? neck : spine3;
                            }
                        }
                    }

                    if (cfg.masterEsp) {
                        glm::vec2 neckEsp;
                        uint64_t boneStartEsp = *(uint64_t*)((7 * 0x8) + BipedPose);
                        if (boneStartEsp) {
                            glm::vec3 _neckEsp;
                            MessiahMatrixAdd(*(XMFLOAT3X4*)(boneStartEsp + 0x30), dxTrans, _neckEsp);
                            w2s(bloodstrike::renderer::camera, _neckEsp, neckEsp);
                        }
                        else { neckEsp = result; }

                        float height = std::abs(neckEsp.y - result.y);
                        float width = height * 0.4f;

                        if (cfg.espBox) {
                            ImColor espCol = ImColor(cfg.espBoxColor);
                            visuals::DrawCornerBox(neckEsp.x - width, neckEsp.y, width * 1.8f, height, 1, visuals::ColorToArray(espCol));
                        }

                        if (cfg.espSkeleton) {
                            ImColor skelColor = ImColor(cfg.espSkeletonColor);
                            glm::vec2 s_neck, s_spine1, s_spine2, s_spine3, s_pelvis;
                            glm::vec2 s_sholL, s_elbowL, s_wristL, s_sholR, s_elbowR, s_wristR;
                            glm::vec2 s_buttL, s_kneeL, s_footL, s_buttR, s_kneeR, s_footR;
                            glm::vec3 temp;
                            auto getBone = [&](int idx, glm::vec2& out) {
                                uint64_t bs = *(uint64_t*)((idx * 0x8) + BipedPose);
                                if (bs) { MessiahMatrixAdd(*(XMFLOAT3X4*)(bs + 0x30), dxTrans, temp); w2s(bloodstrike::renderer::camera, temp, out); }
                                };
                            getBone(7, s_neck); getBone(6, s_spine1); getBone(5, s_spine2); getBone(4, s_spine3); getBone(3, s_pelvis);
                            getBone(14, s_sholL); getBone(15, s_elbowL); getBone(16, s_wristL);
                            getBone(9, s_sholR); getBone(10, s_elbowR); getBone(11, s_wristR);
                            getBone(22, s_buttL); getBone(23, s_kneeL); getBone(24, s_footL);
                            getBone(18, s_buttR); getBone(19, s_kneeR); getBone(20, s_footR);

                            BoneConnection(s_neck, s_spine1, skelColor); BoneConnection(s_spine1, s_spine2, skelColor);
                            BoneConnection(s_spine2, s_spine3, skelColor); BoneConnection(s_spine3, s_pelvis, skelColor);
                            BoneConnection(s_spine1, s_sholL, skelColor); BoneConnection(s_sholL, s_elbowL, skelColor); BoneConnection(s_elbowL, s_wristL, skelColor);
                            BoneConnection(s_spine1, s_sholR, skelColor); BoneConnection(s_sholR, s_elbowR, skelColor); BoneConnection(s_elbowR, s_wristR, skelColor);
                            BoneConnection(s_pelvis, s_buttL, skelColor); BoneConnection(s_buttL, s_kneeL, skelColor); BoneConnection(s_kneeL, s_footL, skelColor);
                            BoneConnection(s_pelvis, s_buttR, skelColor); BoneConnection(s_buttR, s_kneeR, skelColor); BoneConnection(s_kneeR, s_footR, skelColor);
                        }

                        if (cfg.espDistance) {
                            float z = std::clamp(height * 0.25f, 15.0f, 40.0f);
                            std::string dist_txt = std::format("{:d}m", dst_m);
                            visuals::DrawLabel(dist_txt, glm::vec2(neckEsp.x, neckEsp.y - (z)), visuals::ColorToArray(ImColor(255, 255, 255, 255)), true);
                        }
                    }
                }
            }
            currentActor = *(uint64_t*)(currentActor);
        } while (currentActor != head);

        if (cfg.masterAimbot && cfg.aimbotDrawFov) {
            ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(sc.x, sc.y), cfg.aimbotFov, ImColor(cfg.aimbotFovColor), 64, 1.0f);
        }

        if (cfg.masterAimbot && closest_dst < cfg.aimbotFov && target_pos.x != -1.0f && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0)
        {
            POINT target = { (LONG)target_pos.x, (LONG)target_pos.y };
            ClientToScreen(bloodstrike::renderer::hWindow, &target);
            POINT cur; GetCursorPos(&cur);

            float deltaX = (float)(target.x - cur.x);
            float deltaY = (float)(target.y - cur.y);

            float smoothVal = cfg.aimbotSmooth;
            if (smoothVal < 1.0f) smoothVal = 1.0f;

            deltaX /= smoothVal;
            deltaY /= smoothVal;

            if (std::abs(deltaX) < 1.0f && std::abs(deltaX) > 0.1f) deltaX = (deltaX > 0) ? 1.0f : -1.0f;
            if (std::abs(deltaY) < 1.0f && std::abs(deltaY) > 0.1f) deltaY = (deltaY > 0) ? 1.0f : -1.0f;

            hooks::dx = static_cast<LONG>(deltaX);
            hooks::dy = static_cast<LONG>(deltaY);

            if (hooks::dx != 0 || hooks::dy != 0) {
                hooks::should_change_mouse = true;
                // Cast explícito para argumentos mouse_event
                mouse_event(MOUSEEVENTF_MOVE, 0, 0, static_cast<DWORD>(0), static_cast<ULONG_PTR>(0));
                hooks::targetAddr = lastTarget;
            }
            else {
                hooks::should_change_mouse = false;
            }

            if (cfg.aimbotSnaplines) {
                ImGui::GetBackgroundDrawList()->AddLine(ImVec2(sc.x, sc.y), ImVec2(target_pos.x, target_pos.y), ImColor(cfg.aimbotFovColor), 1.0f);
            }
        }
        else
        {
            hooks::should_change_mouse = false;
            closest_dst = FLT_MAX;
        }
    }
}

void SetupStyles()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;
    style.WindowPadding = ImVec2(15, 15);
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(12, 8);

    if (cfg.currentThemeIdx == 0) // Cyber Dark
    {
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.14f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.19f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.27f, 0.54f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.40f, 0.26f, 0.68f, 0.40f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.54f, 0.26f, 0.86f, 0.67f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.29f, 0.18f, 0.47f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.78f, 0.40f, 0.99f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.54f, 0.26f, 0.86f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.78f, 0.40f, 0.99f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.29f, 0.18f, 0.47f, 0.65f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.26f, 0.68f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.54f, 0.26f, 0.86f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.40f, 0.26f, 0.68f, 0.45f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.54f, 0.26f, 0.86f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.78f, 0.40f, 0.99f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.25f, 0.86f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.26f, 0.68f, 0.80f);
        colors[ImGuiCol_TabActive] = ImVec4(0.29f, 0.18f, 0.47f, 1.00f);
    }
    else if (cfg.currentThemeIdx == 1) // Midnight
    {
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 0.98f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        style.WindowRounding = 4.0f;
    }
}

HRESULT hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if (!bloodstrike::renderer::hooked)
    {
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&bloodstrike::renderer::deviceInstance)))
            return oPresent(pSwapChain, SyncInterval, Flags);

        bloodstrike::renderer::deviceInstance->GetImmediateContext(&bloodstrike::renderer::contextInstance);

        ID3D11Texture2D* backBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        if (!backBuffer) return oPresent(pSwapChain, SyncInterval, Flags);

        bloodstrike::renderer::deviceInstance->CreateRenderTargetView(backBuffer, nullptr, &bloodstrike::renderer::rtv);
        backBuffer->Release();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        SetupStyles();
        bloodstrike::renderer::hWindow = *(HWND*)(base + bloodstrike::renderer::hwnd);

        if (!initHookInput) {
            oWndProc = (WNDPROC)SetWindowLongPtr(bloodstrike::renderer::hWindow, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
            initHookInput = true;
        }

        ImGui_ImplWin32_Init(bloodstrike::renderer::hWindow);
        ImGui_ImplDX11_Init(bloodstrike::renderer::deviceInstance, bloodstrike::renderer::contextInstance);

        bloodstrike::renderer::hooked = true;

        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    if (hooks::g_needsResize) {
        ID3D11Texture2D* backBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        if (backBuffer) {
            bloodstrike::renderer::deviceInstance->CreateRenderTargetView(backBuffer, nullptr, &bloodstrike::renderer::rtv);
            backBuffer->Release(); ImGui_ImplDX11_InvalidateDeviceObjects(); ImGui_ImplDX11_CreateDeviceObjects();
        }
        D3D11_VIEWPORT vp; vp.TopLeftX = 0; vp.TopLeftY = 0; vp.Width = static_cast<FLOAT>(hooks::g_newWidth); vp.Height = static_cast<FLOAT>(hooks::g_newHeight); vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        bloodstrike::renderer::contextInstance->RSSetViewports(1, &vp);
        ImGui::GetIO().DisplaySize.x = static_cast<float>(hooks::g_newWidth); ImGui::GetIO().DisplaySize.y = static_cast<float>(hooks::g_newHeight);
        hooks::g_needsResize = false;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if ((GetAsyncKeyState(VK_INSERT) & 1) || (GetAsyncKeyState(VK_BACK) & 1))
    {
        menuOpen = !menuOpen;
        if (menuOpen) ImGui::GetIO().MouseDrawCursor = true;
        else ImGui::GetIO().MouseDrawCursor = false;
    }

    ImGuiIO& io = ImGui::GetIO();
    float fadeSpeed = io.DeltaTime * 8.0f;
    if (menuOpen) menuAlpha = ImMin(menuAlpha + fadeSpeed, 1.0f);
    else menuAlpha = ImMax(menuAlpha - fadeSpeed, 0.0f);

    if (menuAlpha > 0.0f)
    {
        SetupStyles();
        ImGui::GetStyle().Alpha = menuAlpha;
        ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(" BLOOODSTRIKE INTERNAL ", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::BeginChild("##Sidebar", ImVec2(160, 0), true);
            {
                ImGui::Spacing();
                ImVec2 btnSize = ImVec2(ImGui::GetContentRegionAvail().x, 45);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.1f, 0.5f));

                if (currentTab == 0) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                if (ImGui::Button("  AIMBOT", btnSize)) currentTab = 0;
                if (currentTab == 0) ImGui::PopStyleColor();
                ImGui::Spacing();

                if (currentTab == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                if (ImGui::Button("  VISUALS", btnSize)) currentTab = 1;
                if (currentTab == 1) ImGui::PopStyleColor();
                ImGui::Spacing();

                if (currentTab == 2) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                if (ImGui::Button("  MISC", btnSize)) currentTab = 2;
                if (currentTab == 2) ImGui::PopStyleColor();
                ImGui::Spacing();

                if (currentTab == 3) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                if (ImGui::Button("  CONFIG", btnSize)) currentTab = 3;
                if (currentTab == 3) ImGui::PopStyleColor();

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("##ContentRegion", ImVec2(0, 0), true);
            {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);

                switch (currentTab)
                {
                case 0:
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CONFIGURAÇÕES DE MIRA");
                    ImGui::Separator(); ImGui::Spacing();

                    ImGui::Checkbox("Ativar Aimbot", &cfg.masterAimbot);
                    if (cfg.masterAimbot)
                    {
                        ImGui::Indent();
                        ImGui::SliderFloat("Campo de Visão (FOV)", &cfg.aimbotFov, 35.0f, 500.0f, "%.0f px");
                        ImGui::SliderFloat("Suavização (Smooth)", &cfg.aimbotSmooth, 1.0f, 20.0f, "%.1f");
                        ImGui::SliderFloat("Distância Máxima", &cfg.aimbotMaxDist, 10.0f, 1000.0f, "%.0f m");

                        ImGui::Spacing();
                        ImGui::Text("Seleção de Alvo:");
                        ImGui::RadioButton("Cabeça", &cfg.aimLocation, 0); ImGui::SameLine();
                        ImGui::RadioButton("Corpo", &cfg.aimLocation, 1); ImGui::SameLine();
                        ImGui::RadioButton("Aleatório", &cfg.aimLocation, 2);
                        if (cfg.aimLocation == 2) {
                            ImGui::SliderFloat("Chance Cabeça", &cfg.aimbotHeadChance, 0.0f, 1.0f, "%.0f%%");
                        }

                        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                        ImGui::Text("Visuais do Aimbot:");
                        ImGui::Checkbox("Desenhar Círculo FOV", &cfg.aimbotDrawFov);
                        if (cfg.aimbotDrawFov) ImGui::ColorEdit4("Cor do FOV", (float*)&cfg.aimbotFovColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                        ImGui::Checkbox("Desenhar Snaplines", &cfg.aimbotSnaplines);

                        ImGui::Unindent();
                    }
                    break;

                case 1:
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CONFIGURAÇÕES DE ESP");
                    ImGui::Separator(); ImGui::Spacing();

                    ImGui::Checkbox("Ativar ESP", &cfg.masterEsp);
                    if (cfg.masterEsp)
                    {
                        ImGui::Indent();
                        ImGui::Checkbox("Box 2D", &cfg.espBox);
                        ImGui::SameLine();
                        ImGui::ColorEdit4("Cor Box##pick", (float*)&cfg.espBoxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

                        ImGui::Checkbox("Esqueleto", &cfg.espSkeleton);
                        ImGui::SameLine();
                        ImGui::ColorEdit4("Cor Esq.##pick", (float*)&cfg.espSkeletonColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

                        ImGui::Spacing();
                        ImGui::Text("Informações:");
                        ImGui::Checkbox("Distância", &cfg.espDistance);
                        ImGui::Unindent();
                    }
                    break;

                case 2:
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "OUTRAS OPÇÕES");
                    ImGui::Separator(); ImGui::Spacing();

                    ImGui::Text("Presets:");
                    if (ImGui::Button("Legit", ImVec2(100, 25))) {
                        cfg.aimLocation = 2; cfg.aimbotHeadChance = 0.2f; cfg.aimbotSmooth = 12.0f; cfg.aimbotFov = 120.0f; cfg.aimbotDrawFov = false; cfg.aimbotMaxDist = 150.0f;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Rage", ImVec2(100, 25))) {
                        cfg.aimLocation = 0; cfg.aimbotHeadChance = 0.0f; cfg.aimbotSmooth = 1.0f; cfg.aimbotFov = 500.0f; cfg.aimbotDrawFov = true; cfg.aimbotMaxDist = 1000.0f;
                    }
                    ImGui::Spacing();
                    ImGui::SliderFloat("Menu Anim Speed", &cfg.animSpeed, 0.01f, 0.2f, "%.2f");
                    break;

                case 3:
                    RenderConfigTab();
                    break;
                }
                ImGui::PopItemWidth();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    CheatTick();

    ImGui::Render();
    bloodstrike::renderer::contextInstance->OMSetRenderTargets(1, &bloodstrike::renderer::rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(pSwapChain, SyncInterval, Flags);
}

void RenderConfigTab()
{
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "SISTEMA");
    ImGui::Separator(); ImGui::Spacing();

    ImGui::Text("Tema:");
    const char* themes[] = { "Cyber Dark (Roxo)", "Midnight (Preto)" };
    if (ImGui::Combo("Selecionar", &cfg.currentThemeIdx, themes, IM_ARRAYSIZE(themes)))
    {
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (ImGui::Button("Salvar Config", ImVec2(150, 30)))
    {
        ImGui::OpenPopup("SaveSuccess");
    }

    ImGui::SameLine();

    if (ImGui::Button("Carregar Config", ImVec2(150, 30)))
    {
    }

    if (ImGui::BeginPopupModal("SaveSuccess", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configuração salva!"); ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

void Thread(HMODULE hModule)
{
    if (!findPresent() || MH_Initialize() != MH_OK) { cleanup(hModule); return; }

    MH_STATUS status = MH_CreateHook((LPVOID)aPresent, &hkPresent, (void**)&oPresent); status = MH_EnableHook((LPVOID)aPresent);

    status = MH_CreateHook((LPVOID)aResizeBuffers, &hooks::hkResizeBuffers, (void**)&oResizeBuffers); status = MH_EnableHook((LPVOID)aResizeBuffers);

    HMODULE hUser32 = GetModuleHandleW(L"user32.dll"); if (!hUser32) hUser32 = LoadLibraryW(L"user32.dll"); if (!hUser32) { cleanup(hModule); return; }
    aGetRawInputData = (uint64_t)GetProcAddress(hUser32, "GetRawInputData");

    status = MH_CreateHook((LPVOID)aGetRawInputData, &hooks::hkGetRawInputData, (void**)&oGetRawInputData); status = MH_EnableHook((LPVOID)aGetRawInputData);

    while (!GetAsyncKeyState(VK_F6)) { std::this_thread::sleep_for(std::chrono::milliseconds(40)); }
    if (initHookInput) SetWindowLongPtr(bloodstrike::renderer::hWindow, GWLP_WNDPROC, (LONG_PTR)oWndProc);
    cleanup(hModule);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        base = (uint64_t)GetModuleHandleA(NULL);
#ifdef DEBUG
        AllocConsole(); freopen_s(&f, "CONOUT$", "w", stdout); freopen_s(&f, "CONIN$", "r", stdin); printf("[+] init\n");
#endif
        CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Thread, hModule, NULL, NULL); break;
    case DLL_THREAD_ATTACH: case DLL_THREAD_DETACH: case DLL_PROCESS_DETACH: break;
    }
    return TRUE;
}