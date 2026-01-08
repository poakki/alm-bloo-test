#pragma once
#include <vector>
#include <string>

// Adicione estas variáveis globais para controle
//inline bool menuOpen = false;

// Aimbot Configs
inline bool aimbotEnabled = true;
inline bool usePrediction = true; // Ativa o predict
inline bool aimBody = false;
inline bool randomizeAim = true;
inline float headChance = 0.1855f;
inline float fov = 175.0f;
inline float smooth = 5.0f; // Suavização para parecer legit
inline float bulletSpeed = 650.0f; // Velocidade média da bala (ajuste conforme a arma)

// Visuals / Evasion
inline bool streamProof = false; // Se ativado, não desenha o ESP (para lives)
inline bool drawFOV = true;
inline bool aimbotDebug = false;
inline bool debugStats = false;
inline bool kingKongEsp = false;

// Input / Mouse (Mantido para compatibilidade, mas o aimbot principal não dependerá disso)
inline bool should_change_mouse = false;
inline char dx = 0;
inline char dy = 0;