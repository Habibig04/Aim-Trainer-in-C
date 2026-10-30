// aim_trainer.cpp 
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cfloat>
#include <fstream>
#include <algorithm>
#include <string>

//  constants
constexpr float TARGET_R = 0.5f;
constexpr float REFLEX_LIFETIME = 1.5f;
constexpr int MAX_TARGETS = 20;
constexpr int NUM_MODES = 8;
constexpr int NUM_MAPS = 4;
const Vector3 EYE = {0.0f, 1.75f, 0.0f};

enum GameState { MENU_MAIN, MENU_SETUP, MENU_SETTINGS, COUNTDOWN, GAMEPLAY, GAMEPLAY_PAUSED, RESULTS };
enum Quality { Q_POTATO = 0, Q_MEDIUM = 1, Q_HIGH = 2 };
enum MapId { MAP_ARENA = 0, MAP_RANGE, MAP_WINDOW, MAP_ALLEY };

struct ModeInfo {
    const char* name;
    const char* desc;
    float duration;
    int count;
    bool moving;
    bool reflex;
    bool bots;
};

const ModeInfo MODES[NUM_MODES] = {
    {"Static",      "Stationary targets",          0,   10, false, false, false},
    {"Moving",      "Strafing targets",            0,   8,  true,  false, false},
    {"Reflex",      "Shrinking targets",           0,   3,  false, true,  false},
    {"Bots",        "Strafing bots, aim for head", 0,   4,  true,  false, true},
    {"30 Seconds",  "Static sprint",               30,  10, false, false, false},
    {"60 Seconds",  "Static, one minute",          60,  10, false, false, false},
    {"120 Seconds", "Static, two minutes",         120, 10, false, false, false},
    {"Endless",     "Moving targets, no limit",    0,   8,  true,  false, false},
};

const char* const QUALITY_NAMES[3] = {"Potato", "Medium", "High"};
const int FPS_OPTS[5] = {60, 120, 144, 240, 0};
const char* const FPS_NAMES[5] = {"60", "120", "144", "240", "Unlimited"};
const int RES_OPTS[5][2] = {{1024, 576}, {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}};
const char* const RES_NAMES[5] = {"1024x576", "1280x720", "1600x900", "1920x1080", "2560x1440"};
const char* const DISPLAY_NAMES[2] = {"Windowed", "Fullscreen"};
const char* const CH_STYLE_NAMES[4] = {"Cross", "Dot", "Circle", "Cross + Dot"};
const char* const CH_COLOR_NAMES[8] = {"Green", "Cyan", "White", "Red", "Yellow", "Pink", "Orange", "Custom"};
const Color CH_COLORS[7] = {{0, 255, 120, 255}, {0, 230, 255, 255}, {255, 255, 255, 255}, {255, 60, 60, 255},
                            {255, 230, 50, 255}, {255, 80, 220, 255}, {255, 150, 30, 255}};
const char* const TARGET_COLOR_NAMES[7] = {"Red", "Green", "Cyan", "Pink", "Yellow", "Orange", "Mixed"};
const Color TARGET_COLORS[6] = {{235, 55, 65, 255}, {60, 220, 110, 255}, {40, 200, 255, 255},
                                {255, 90, 200, 255}, {255, 215, 60, 255}, {255, 140, 30, 255}};
const char* const TAB_NAMES[3] = {"GENERAL", "GRAPHICS", "CROSSHAIR"};

//  data types
struct CrosshairCfg {
    int style = 3;
    int size = 8;
    int thickness = 2;
    int gap = 4;
    int color = 0;
    float hue = 120.0f;
    int opacity = 100;
    bool outline = true;
};

struct Settings {
    float sens = 3.0f;
    float fov = 75.0f;
    float volume = 70.0f;
    int w = 1280, h = 720;
    int quality = Q_HIGH;
    int fpsIdx = 1;
    int targetColor = 0;
    int lastMap = 0;
    bool fullscreen = false;
    bool showFps = true;
    CrosshairCfg ch;

    void sanitize() {
        sens = Clamp(sens, 0.1f, 15.0f);
        fov = Clamp(fov, 50.0f, 120.0f);
        volume = Clamp(volume, 0.0f, 100.0f);
        if (w < 640 || h < 360 || w > 7680 || h > 4320) { w = 1280; h = 720; }
        quality = std::clamp(quality, 0, 2);
        fpsIdx = std::clamp(fpsIdx, 0, 4);
        targetColor = std::clamp(targetColor, 0, 6);
        lastMap = std::clamp(lastMap, 0, NUM_MAPS - 1);
        ch.style = std::clamp(ch.style, 0, 3);
        ch.size = std::clamp(ch.size, 1, 30);
        ch.thickness = std::clamp(ch.thickness, 1, 8);
        ch.gap = std::clamp(ch.gap, 0, 20);
        ch.color = std::clamp(ch.color, 0, 7);
        ch.hue = Clamp(ch.hue, 0.0f, 360.0f);
        ch.opacity = std::clamp(ch.opacity, 20, 100);
    }
    void save() const {
        std::ofstream f("settings.ini");
        if (!f) return;
        f << "sens " << sens << "\nfov " << fov << "\nvolume " << volume << "\nw " << w << "\nh " << h
          << "\nquality " << quality << "\nfps " << fpsIdx << "\ntargetColor " << targetColor
          << "\nlastMap " << lastMap << "\nfullscreen " << fullscreen << "\nshowFps " << showFps
          << "\nchStyle " << ch.style << "\nchSize " << ch.size << "\nchThick " << ch.thickness
          << "\nchGap " << ch.gap << "\nchColor " << ch.color << "\nchHue " << ch.hue
          << "\nchOpacity " << ch.opacity << "\nchOutline " << ch.outline << "\n";
    }
    bool load() {
        std::ifstream f("settings.ini");
        if (!f) return false;
        std::string k;
        double v;
        while (f >> k >> v) {
            if (k == "sens") sens = (float)v;
            else if (k == "fov") fov = (float)v;
            else if (k == "volume") volume = (float)v;
            else if (k == "w") w = (int)v;
            else if (k == "h") h = (int)v;
            else if (k == "quality") quality = (int)v;
            else if (k == "fps") fpsIdx = (int)v;
            else if (k == "targetColor") targetColor = (int)v;
            else if (k == "lastMap") lastMap = (int)v;
            else if (k == "fullscreen") fullscreen = v != 0;
            else if (k == "showFps") showFps = v != 0;
            else if (k == "chStyle") ch.style = (int)v;
            else if (k == "chSize") ch.size = (int)v;
            else if (k == "chThick") ch.thickness = (int)v;
            else if (k == "chGap") ch.gap = (int)v;
            else if (k == "chColor") ch.color = (int)v;
            else if (k == "chHue") ch.hue = (float)v;
            else if (k == "chOpacity") ch.opacity = (int)v;
            else if (k == "chOutline") ch.outline = v != 0;
        }
        sanitize();
        return true;
    }
};

struct Target {
    Vector3 pos{0, 0, 0}, vel{0, 0, 0}, lo{0, 0, 0}, hi{0, 0, 0}, anchor{0, 0, 0};
    float age = 0, moveTimer = 0, moveInterval = 2, speed = 4;
    bool bot = false;
    int colorIdx = 0;
};

struct Particle {
    Vector3 pos, vel;
    float life, maxLife, size;
    Color col;
};

struct Popup {
    Vector3 pos;
    float life;
    int pts;
    bool head;
};

struct GameStats {
    int hits = 0, shots = 0, expired = 0, score = 0, streak = 0, bestStreak = 0;
    float sessionTime = 0, killTimeTotal = 0;
    float accuracy() const { return shots > 0 ? 100.0f * hits / shots : 0.0f; }
    float tpm() const { return sessionTime > 0.1f ? hits / sessionTime * 60.0f : 0.0f; }
    float avgKill() const { return hits > 0 ? killTimeTotal / hits : 0.0f; }
};

struct MapDef {
    const char* name;
    const char* desc;
    Vector3 boxLo, boxHi;
    float botFloor, ts, slotRangeX;
    Color skyTop, skyBot;
    std::vector<Vector3> sphereSlots, botSlots;
};

//  globals
MapDef MAPS[NUM_MAPS];
Settings settings;
GameState gameState = MENU_MAIN;
int selectedMap = 0, gameMap = 0, gameMode = 0;
Camera3D camera, menuCam;
std::vector<Target> targets, menuTargets;
std::vector<Particle> particles, menuParticles;
std::vector<Popup> popups;
GameStats stats;
int bestScore[NUM_MAPS][NUM_MODES] = {};
bool newBest = false, quitRequested = false, fsActive = false;

float yaw = 0, pitch = 0, timeLeft = 0, countdownTimer = 0, hitMarker = 0, crossKick = 0;
float perfWarm = 0, perfTime = 0, toast = 0, focusGrace = 0, menuIntro = 0, menuTime = 0;
int skipDelta = 0, countdownValue = 3, perfFrames = 0, settingsTab = 0, menuMap = 0, demoTarget = -1;
float demoCooldown = 0;
bool hitMarkerHead = false, perfChecked = false;
Vector2 demoPos = {0, 0};
const char* toastMsg = "";

bool audioOk = false;
Sound hitSound, headSound, shootSound, missSound;

float ui = 1.0f;
const void* activeCtl = nullptr;

//  utility
float RandF(float a, float b) { return a + (float)rand() / (float)RAND_MAX * (b - a); }

Color Mix(Color a, Color b, float t) {
    return Color{(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
Color Dark(Color c, float f) { return Color{(unsigned char)(c.r * f), (unsigned char)(c.g * f), (unsigned char)(c.b * f), c.a}; }
float EaseOut(float t) { t = Clamp(t, 0.0f, 1.0f); return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }

void Slab(float x0, float x1, float y0, float y1, float z0, float z1, Color c) {
    DrawCube({(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2}, x1 - x0, y1 - y0, z1 - z0, c);
}

const char* GLName() {
    switch (rlGetVersion()) {
        case RL_OPENGL_11: return "OpenGL 1.1";
        case RL_OPENGL_21: return "OpenGL 2.1";
        case RL_OPENGL_33: return "OpenGL 3.3";
        case RL_OPENGL_43: return "OpenGL 4.3";
        case RL_OPENGL_ES_20: return "OpenGL ES 2.0";
        case RL_OPENGL_ES_30: return "OpenGL ES 3.0";
        default: return "OpenGL";
    }
}

//  audio
Sound MakeBeep(float freq, float dur, float vol) {
    const int sr = 44100;
    int n = (int)(sr * dur);
    short* d = (short*)malloc(n * sizeof(short));
    for (int i = 0; i < n; i++) {
        float env = 1.0f - (float)i / n;
        d[i] = (short)(sinf(2.0f * PI * freq * i / sr) * 32000.0f * vol * env);
    }
    Wave w = {(unsigned int)n, (unsigned int)sr, 16, 1, d};
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}
void Play(Sound& s) { if (audioOk) PlaySound(s); }

//  scores
void LoadScores() {
    std::ifstream f("scores.ini");
    int m, d, s;
    while (f >> m >> d >> s)
        if (m >= 0 && m < NUM_MAPS && d >= 0 && d < NUM_MODES) bestScore[m][d] = s;
}
void SaveScores() {
    std::ofstream f("scores.ini");
    if (!f) return;
    for (int m = 0; m < NUM_MAPS; m++)
        for (int d = 0; d < NUM_MODES; d++) f << m << " " << d << " " << bestScore[m][d] << "\n";
}

//  maps
void InitMaps() {
    MAPS[MAP_ARENA] = {"Arena", "Open training room", {-11, 1.5f, 14}, {11, 9, 40}, 0, 1.0f, 1.0f,
                       {12, 14, 28, 255}, {34, 40, 70, 255}, {}, {}};
    MAPS[MAP_RANGE] = {"Firing Range", "Shooting booth, targets down the lane", {-7, 1.0f, 12}, {7, 4.5f, 42}, 0, 1.2f, 1.0f,
                       {80, 140, 210, 255}, {200, 225, 245, 255}, {}, {}};
    MAPS[MAP_WINDOW] = {"Apartment", "Stand in a flat, targets sit in the windows across the street", {0, 0, 0}, {0, 0, 0}, 0, 1.5f, 0.45f,
                        {40, 36, 80, 255}, {250, 150, 100, 255}, {}, {}};
    MAPS[MAP_ALLEY] = {"Dust Alley", "Sandy alley with crates and a doorway", {-6, 2.0f, 14}, {6, 6, 38}, 0, 1.0f, 0.9f,
                       {70, 130, 210, 255}, {245, 220, 175, 255}, {}, {}};
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 7; c++) {
            float x = -12.0f + c * 4.0f, y = 4.0f + r * 4.0f;
            MAPS[MAP_WINDOW].sphereSlots.push_back({x, y, 27.2f});
            MAPS[MAP_WINDOW].botSlots.push_back({x, y - 1.45f, 27.2f});
        }
    MAPS[MAP_ALLEY].botSlots = {{-3.9f, 0, 24}, {4.5f, 0, 20}, {-6.6f, 0, 31}, {6.4f, 0, 33}, {1.4f, 0, 36.5f}, {0, 0, 41.5f}};
}

void DrawArenaMap(int q) {
    Slab(-15, 15, -1, 0, -6, 46, {48, 50, 62, 255});
    if (q >= Q_MEDIUM)
        for (int x = 0; x < 6; x++)
            for (int z = 0; z < 10; z++)
                if ((x + z) % 2 == 0) Slab(-15 + x * 5.0f, -10 + x * 5.0f, 0, 0.02f, -5 + z * 5.0f, z * 5.0f, {58, 60, 74, 255});
    Slab(-16, -15, 0, 14, -7, 47, {28, 36, 70, 255});
    Slab(15, 16, 0, 14, -7, 47, {28, 36, 70, 255});
    Slab(-16, 16, 0, 14, -7, -6, {24, 30, 60, 255});
    Slab(-16, 16, 0, 14, 46, 47, {44, 60, 118, 255});
    Slab(-16, 16, 14, 15, -7, 47, {30, 30, 40, 255});
    if (q >= Q_MEDIUM) {
        for (int i = 0; i < 4; i++) Slab(-9 + i * 6.0f, -6 + i * 6.0f, 13.9f, 14.0f, 8, 40, {200, 220, 255, 255});
        Slab(-12, -4, 3, 9, 45.9f, 46.0f, {70, 130, 255, 255});
        Slab(4, 12, 3, 9, 45.9f, 46.0f, {70, 130, 255, 255});
    }
}

void DrawRangeMap(int q) {
    Slab(-12, 12, -1, 0, -9, 51, {62, 62, 68, 255});
    Slab(-13, -12, 0, 10, -9, 51, {52, 58, 52, 255});
    Slab(12, 13, 0, 10, -9, 51, {52, 58, 52, 255});
    Slab(-13, 13, 0, 10, -10, -9, {44, 48, 44, 255});
    Slab(-12, 12, 0, 5, 46, 50, {92, 72, 46, 255});
    Slab(-12, 12, 0, 10, 50, 51, {34, 34, 38, 255});
    Slab(-1.6f, 1.6f, 0, 1.1f, 1.2f, 1.6f, {90, 90, 100, 255});
    Slab(-1.7f, 1.7f, 1.1f, 1.2f, 1.1f, 1.7f, {125, 125, 135, 255});
    Slab(-2.2f, -1.8f, 0, 3, -3, 1.7f, {70, 74, 84, 255});
    Slab(1.8f, 2.2f, 0, 3, -3, 1.7f, {70, 74, 84, 255});
    Slab(-2.2f, 2.2f, 3, 3.3f, -3, 1.7f, {60, 64, 74, 255});
    if (q >= Q_MEDIUM) {
        for (int i = 1; i <= 4; i++) {
            float z = i * 10.0f;
            Slab(-11.95f, -11.0f, 1.8f, 3.4f, z - 0.9f, z + 0.9f, {235, 205, 45, 255});
            Slab(11.0f, 11.95f, 1.8f, 3.4f, z - 0.9f, z + 0.9f, {235, 205, 45, 255});
            Slab(-11.5f, -11.4f, 0, 1.8f, z - 0.1f, z + 0.1f, {80, 80, 86, 255});
            Slab(11.4f, 11.5f, 0, 1.8f, z - 0.1f, z + 0.1f, {80, 80, 86, 255});
        }
        Slab(-6, 6, 0, 0.02f, 47, 47.4f, {235, 205, 45, 255});
    }
}

void DrawWindowMap(int q) {
    Slab(-10, 10, -1, 0, -6, 3.5f, {112, 82, 56, 255});
    Slab(-11, 11, 0, 5.3f, -7, -6, {205, 200, 190, 255});
    Slab(-11, -10, 0, 5.3f, -7, 3.5f, {198, 192, 182, 255});
    Slab(10, 11, 0, 5.3f, -7, 3.5f, {198, 192, 182, 255});
    Slab(-11, 11, 5, 5.3f, -7, 3.5f, {228, 228, 228, 255});
    Slab(-11, 11, 0, 0.9f, 3, 3.5f, {200, 195, 185, 255});
    Slab(-11, 11, 4.4f, 5.0f, 3, 3.5f, {200, 195, 185, 255});
    Slab(-11, -9, 0.9f, 4.4f, 3, 3.5f, {200, 195, 185, 255});
    Slab(9, 11, 0.9f, 4.4f, 3, 3.5f, {200, 195, 185, 255});
    Slab(-9, 9, 0.9f, 1.0f, 2.7f, 3.7f, {230, 225, 215, 255});
    if (q >= Q_MEDIUM) {
        Slab(-8, -5, 0, 0.5f, -5.5f, -3.5f, {72, 62, 94, 255});
        Slab(-8, -5, 0.5f, 1.1f, -5.8f, -5.4f, {64, 54, 86, 255});
        Slab(5, 8, 0, 0.7f, -3, -1, {92, 66, 46, 255});
        DrawCylinder({6.5f, 0.7f, -2.0f}, 0.05f, 0.05f, 0.8f, 8, {60, 60, 60, 255});
        DrawSphereEx({6.5f, 1.65f, -2.0f}, 0.28f, 10, 10, {255, 230, 170, 255});
    }
    Slab(-80, 80, -1, 0, 3.5f, 70, {40, 42, 48, 255});
    Slab(-18, 18, 0, 26, 28, 40, {58, 52, 64, 255});
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 7; c++) {
            float x = -12.0f + c * 4.0f, y = 4.0f + r * 4.0f;
            bool lit = ((c * 7 + r * 13) % 5 == 0);
            Slab(x - 1.5f, x + 1.5f, y - 1.7f, y + 1.7f, 27.92f, 28.0f, {34, 30, 42, 255});
            Slab(x - 1.3f, x + 1.3f, y - 1.5f, y + 1.5f, 27.84f, 27.92f, lit ? Color{120, 100, 62, 255} : Color{20, 24, 40, 255});
        }
    if (q >= Q_MEDIUM) {
        Slab(-70, -22, 0, 38, 24, 48, {36, 34, 50, 255});
        Slab(22, 70, 0, 32, 26, 50, {36, 34, 50, 255});
        Slab(-40, 40, 0, 44, 52, 66, {30, 28, 44, 255});
        for (int i = 0; i < 2; i++) {
            float x = i == 0 ? -10.0f : 10.0f;
            DrawCylinder({x, 0, 12}, 0.12f, 0.12f, 6, 8, {50, 50, 56, 255});
            DrawSphereEx({x, 6.2f, 12}, 0.4f, 10, 10, {255, 220, 150, 255});
        }
    }
    if (q >= Q_HIGH) {
        for (int i = 0; i < 40; i++) {
            int side = i % 2;
            float x = side ? 24.0f + (i * 7 % 40) : -24.0f - (i * 7 % 40);
            float y = 3.0f + (i * 5 % 28);
            Slab(x, x + 1.4f, y, y + 1.8f, 23.9f, 24.0f, {255, 210, 120, 255});
        }
    }
}

void DrawAlleyMap(int q) {
    Slab(-9, 9, -1, 0, -9, 48, {192, 168, 122, 255});
    Slab(-10, -8, 0, 9, -9, 50, {176, 146, 100, 255});
    Slab(8, 10, 0, 9, -9, 50, {176, 146, 100, 255});
    Slab(-10, 10, 0, 9, -10, -9, {160, 132, 92, 255});
    Slab(-9, 9, 0, 10, 48, 50, {162, 134, 94, 255});
    Slab(-2, 2, 0, 4.2f, 47.9f, 48, {26, 20, 18, 255});
    Slab(-2.5f, -2, 0, 4.5f, 47.7f, 48, {124, 98, 68, 255});
    Slab(2, 2.5f, 0, 4.5f, 47.7f, 48, {124, 98, 68, 255});
    Slab(-2.5f, 2.5f, 4.2f, 4.7f, 47.7f, 48, {124, 98, 68, 255});
    const float crates[5][2] = {{-2.0f, 24}, {2.8f, 20}, {-5.0f, 31}, {4.8f, 33}, {-0.3f, 36.5f}};
    for (int i = 0; i < 5; i++) {
        float cx = crates[i][0], cz = crates[i][1];
        Slab(cx - 0.7f, cx + 0.7f, 0, 1.4f, cz - 0.7f, cz + 0.7f, {142, 98, 52, 255});
        if (q >= Q_MEDIUM) DrawCubeWires({cx, 0.7f, cz}, 1.4f, 1.4f, 1.4f, {84, 56, 28, 255});
    }
    if (q >= Q_MEDIUM) {
        Slab(-5.7f, -4.3f, 1.4f, 2.4f, 30.3f, 31.7f, {150, 106, 58, 255});
        DrawCubeWires({-5.0f, 1.9f, 31}, 1.4f, 1.0f, 1.4f, {84, 56, 28, 255});
        DrawCylinder({6.2f, 0, 26}, 0.5f, 0.5f, 1.1f, 12, {110, 80, 50, 255});
        DrawCylinder({-6.6f, 0, 20}, 0.5f, 0.5f, 1.1f, 12, {98, 72, 46, 255});
        for (int i = 0; i < 4; i++) {
            float z = 8.0f + i * 10.0f;
            Slab(-8.0f, -7.9f, 4, 6.2f, z - 0.9f, z + 0.9f, {40, 32, 28, 255});
            Slab(7.9f, 8.0f, 4, 6.2f, z - 0.9f, z + 0.9f, {40, 32, 28, 255});
        }
    }
}

void DrawMap(int m, int q) {
    switch (m) {
        case MAP_ARENA: DrawArenaMap(q); break;
        case MAP_RANGE: DrawRangeMap(q); break;
        case MAP_WINDOW: DrawWindowMap(q); break;
        default: DrawAlleyMap(q); break;
    }
}

void DrawSky(int m) {
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), MAPS[m].skyTop, MAPS[m].skyBot);
}

//  targets
float Pop(const Target& t) { return 0.3f + 0.7f * EaseOut(t.age / 0.15f); }

float Radius(const Target& t, int map, bool reflex) {
    float r = TARGET_R * MAPS[map].ts * Pop(t);
    if (reflex) r *= 0.35f + 0.65f * Clamp(1.0f - t.age / REFLEX_LIFETIME, 0.0f, 1.0f);
    return r;
}
float BotScale(const Target& t, int map) { return MAPS[map].ts * Pop(t); }

Vector3 TargetCenter(const Target& t, int map) {
    if (!t.bot) return t.pos;
    return {t.pos.x, t.pos.y + 1.2f * BotScale(t, map), t.pos.z};
}

void RandomVel(Target& t) {
    float a = RandF(0, 2 * PI);
    t.vel = {cosf(a) * t.speed, RandF(-0.35f, 0.35f) * t.speed, sinf(a) * t.speed * 0.35f};
}

bool SpawnTarget(std::vector<Target>& list, int mapIdx, bool bot) {
    if ((int)list.size() >= MAX_TARGETS) return false;
    const MapDef& m = MAPS[mapIdx];
    const std::vector<Vector3>& slots = bot ? m.botSlots : m.sphereSlots;
    Target t;
    t.bot = bot;
    t.colorIdx = rand() % 6;
    bool ok = false;
    for (int tries = 0; tries < 60 && !ok; tries++) {
        if (!slots.empty()) {
            Vector3 s = slots[rand() % slots.size()];
            t.pos = s;
            t.anchor = s;
            if (bot) { t.lo = {s.x - m.slotRangeX, s.y, s.z}; t.hi = {s.x + m.slotRangeX, s.y, s.z}; }
            else { t.lo = {s.x - m.slotRangeX * 0.7f, s.y - 0.5f, s.z}; t.hi = {s.x + m.slotRangeX * 0.7f, s.y + 0.5f, s.z}; }
            t.speed = bot ? RandF(0.5f, 1.2f) : RandF(0.5f, 1.4f);
        } else if (bot) {
            float z = RandF(m.boxLo.z, m.boxHi.z);
            t.pos = {RandF(m.boxLo.x + 1, m.boxHi.x - 1), m.botFloor, z};
            t.anchor = t.pos;
            t.lo = {m.boxLo.x + 1, m.botFloor, z};
            t.hi = {m.boxHi.x - 1, m.botFloor, z};
            t.speed = RandF(2.0f, 4.5f);
        } else {
            t.pos = {RandF(m.boxLo.x + 1, m.boxHi.x - 1), RandF(m.boxLo.y + 0.5f, m.boxHi.y - 0.5f), RandF(m.boxLo.z + 1, m.boxHi.z - 1)};
            t.anchor = t.pos;
            t.lo = {m.boxLo.x + 0.8f, m.boxLo.y + 0.6f, m.boxLo.z + 0.8f};
            t.hi = {m.boxHi.x - 0.8f, m.boxHi.y - 0.6f, m.boxHi.z - 0.8f};
            t.speed = RandF(2.0f, 6.0f);
        }
        ok = true;
        float gap = slots.empty() ? 2.5f * m.ts : 0.5f;
        for (const Target& o : list)
            if (Vector3Distance(t.anchor, o.anchor) < gap) { ok = false; break; }
    }
    if (!ok) return false;
    if (bot) { t.vel = {(rand() % 2 ? 1.0f : -1.0f) * t.speed, 0, 0}; t.moveInterval = RandF(0.35f, 1.3f); }
    else { RandomVel(t); t.moveInterval = RandF(1.0f, 3.0f); }
    list.push_back(t);
    return true;
}

void UpdateTargets(std::vector<Target>& list, float dt, bool moving) {
    for (Target& t : list) {
        t.age += dt;
        if (!moving) continue;
        t.moveTimer += dt;
        if (t.bot) {
            if (t.moveTimer >= t.moveInterval) {
                t.moveTimer = 0;
                t.moveInterval = RandF(0.35f, 1.3f);
                t.vel.x = (rand() % 3 == 0) ? 0.0f : (rand() % 2 ? 1.0f : -1.0f) * t.speed;
            }
            t.pos.x += t.vel.x * dt;
            if (t.pos.x < t.lo.x || t.pos.x > t.hi.x) { t.vel.x = -t.vel.x; t.pos.x = Clamp(t.pos.x, t.lo.x, t.hi.x); }
        } else {
            if (t.moveTimer >= t.moveInterval) { t.moveTimer = 0; t.moveInterval = RandF(1.0f, 3.0f); RandomVel(t); }
            t.pos = Vector3Add(t.pos, Vector3Scale(t.vel, dt));
            if (t.pos.x < t.lo.x || t.pos.x > t.hi.x) { t.vel.x = -t.vel.x; t.pos.x = Clamp(t.pos.x, t.lo.x, t.hi.x); }
            if (t.pos.y < t.lo.y || t.pos.y > t.hi.y) { t.vel.y = -t.vel.y; t.pos.y = Clamp(t.pos.y, t.lo.y, t.hi.y); }
            if (t.pos.z < t.lo.z || t.pos.z > t.hi.z) { t.vel.z = -t.vel.z; t.pos.z = Clamp(t.pos.z, t.lo.z, t.hi.z); }
        }
    }
}

struct Hit { int idx = -1; int zone = 0; };  // zone: 0 body, 1 head, 2 legs

Hit CastRay(const std::vector<Target>& list, Ray ray, int map, bool reflex) {
    Hit best;
    float closest = FLT_MAX;
    for (int i = 0; i < (int)list.size(); i++) {
        const Target& t = list[i];
        if (!t.bot) {
            RayCollision c = GetRayCollisionSphere(ray, t.pos, Radius(t, map, reflex));
            if (c.hit && c.distance < closest) { closest = c.distance; best.idx = i; best.zone = 0; }
            continue;
        }
        float k = BotScale(t, map);
        Vector3 p = t.pos;
        RayCollision h = GetRayCollisionSphere(ray, {p.x, p.y + 1.68f * k, p.z}, 0.22f * k);
        if (h.hit && h.distance < closest) { closest = h.distance; best.idx = i; best.zone = 1; }
        BoundingBox torso = {{p.x - 0.47f * k, p.y + 0.85f * k, p.z - 0.2f * k}, {p.x + 0.47f * k, p.y + 1.55f * k, p.z + 0.2f * k}};
        RayCollision b = GetRayCollisionBox(ray, torso);
        if (b.hit && b.distance < closest) { closest = b.distance; best.idx = i; best.zone = 0; }
        BoundingBox legs = {{p.x - 0.21f * k, p.y, p.z - 0.14f * k}, {p.x + 0.21f * k, p.y + 0.85f * k, p.z + 0.14f * k}};
        RayCollision l = GetRayCollisionBox(ray, legs);
        if (l.hit && l.distance < closest) { closest = l.distance; best.idx = i; best.zone = 2; }
    }
    return best;
}

//  effects
void SpawnParticles(std::vector<Particle>& list, Vector3 p, Color c) {
    int n = settings.quality == Q_HIGH ? 24 : (settings.quality == Q_MEDIUM ? 12 : 0);
    for (int i = 0; i < n; i++) {
        Vector3 d = Vector3Normalize({RandF(-1, 1), RandF(-1, 1), RandF(-1, 1)});
        float life = RandF(0.35f, 0.7f);
        list.push_back({p, Vector3Scale(d, RandF(2, 7)), life, life, RandF(0.08f, 0.16f), c});
    }
}

void UpdateParticles(std::vector<Particle>& list, float dt) {
    for (Particle& p : list) {
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        p.vel.y -= 9.0f * dt;
        p.life -= dt;
    }
    list.erase(std::remove_if(list.begin(), list.end(), [](const Particle& p) { return p.life <= 0; }), list.end());
}

void DrawParticles(const std::vector<Particle>& list) {
    for (const Particle& p : list) DrawCube(p.pos, p.size, p.size, p.size, Fade(p.col, p.life / p.maxLife));
}

Color TargetColorOf(const Target& t) {
    int idx = settings.targetColor;
    if (idx >= 6) idx = t.colorIdx;
    return TARGET_COLORS[idx];
}

void DrawBot(const Target& t, int map, Color col, int q) {
    float k = BotScale(t, map);
    Vector3 p = t.pos;
    Color body = Dark(col, 0.85f), legs = Dark(col, 0.5f), head = {255, 215, 130, 255};
    Color edge = Dark(col, 0.3f);
    DrawCube({p.x, p.y + 0.425f * k, p.z}, 0.42f * k, 0.85f * k, 0.28f * k, legs);
    DrawCube({p.x, p.y + 1.2f * k, p.z}, 0.6f * k, 0.7f * k, 0.32f * k, body);
    DrawCube({p.x - 0.4f * k, p.y + 1.2f * k, p.z}, 0.14f * k, 0.62f * k, 0.18f * k, body);
    DrawCube({p.x + 0.4f * k, p.y + 1.2f * k, p.z}, 0.14f * k, 0.62f * k, 0.18f * k, body);
    int det = q == Q_POTATO ? 6 : 12;
    DrawSphereEx({p.x, p.y + 1.68f * k, p.z}, 0.2f * k, det, det, head);
    if (q >= Q_MEDIUM) {
        DrawCubeWires({p.x, p.y + 1.2f * k, p.z}, 0.6f * k, 0.7f * k, 0.32f * k, edge);
        DrawCubeWires({p.x, p.y + 0.425f * k, p.z}, 0.42f * k, 0.85f * k, 0.28f * k, edge);
    }
}

void DrawTargets(const std::vector<Target>& list, int map, bool reflex, int q, const Camera3D& cam) {
    int det = q == Q_HIGH ? 20 : (q == Q_MEDIUM ? 12 : 6);
    bool floorShadow = q >= Q_MEDIUM && MAPS[map].sphereSlots.empty();
    for (const Target& t : list) {
        Color c = TargetColorOf(t);
        if (t.bot) { DrawBot(t, map, c, q); continue; }
        float r = Radius(t, map, reflex);
        if (reflex) c = Mix(c, Color{255, 215, 60, 255}, 1.0f - Clamp(1.0f - t.age / REFLEX_LIFETIME, 0.0f, 1.0f));
        if (floorShadow) {
            float a = Clamp(1.0f - t.pos.y / 12.0f, 0.15f, 0.6f);
            DrawCylinder({t.pos.x, 0.02f, t.pos.z}, r * 1.1f, r * 1.1f, 0.01f, 14, Fade(BLACK, a));
        }
        DrawSphereEx(t.pos, r, det, det, c);
        if (q >= Q_MEDIUM) {
            Vector3 toCam = Vector3Normalize(Vector3Subtract(cam.position, t.pos));
            Vector3 spec = Vector3Add(t.pos, Vector3Add(Vector3Scale(toCam, r * 0.78f), Vector3Scale(cam.up, r * 0.25f)));
            DrawSphereEx(spec, r * 0.3f, 8, 8, Color{255, 255, 255, 210});
        }
    }
    if (q >= Q_HIGH)
        for (const Target& t : list) {
            if (t.bot) continue;
            Color c = TargetColorOf(t);
            DrawSphereEx(t.pos, Radius(t, map, reflex) * 1.45f, 14, 14, Fade(c, 0.14f));
        }
}

//  display
void ApplyDisplay() {
    if (settings.fullscreen == fsActive) return;
    int m = GetCurrentMonitor();
    Vector2 mp = GetMonitorPosition(m);
    int mw = GetMonitorWidth(m), mh = GetMonitorHeight(m);
    if (mw <= 0 || mh <= 0) { mw = GetScreenWidth(); mh = GetScreenHeight(); }
    if (settings.fullscreen) {
        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowPosition((int)mp.x, (int)mp.y);
        SetWindowSize(mw, mh);
    } else {
        ClearWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(settings.w, settings.h);
        SetWindowPosition((int)mp.x + (mw - settings.w) / 2, (int)mp.y + (mh - settings.h) / 2);
    }
    fsActive = settings.fullscreen;
    focusGrace = 0.7f;
    if (gameState == COUNTDOWN || gameState == GAMEPLAY) { DisableCursor(); skipDelta = 2; }
}

void ToggleFS() { settings.fullscreen = !settings.fullscreen; ApplyDisplay(); }

//  UI helpers (design space is 1280x720, scaled by `ui`)
float SW() { return (float)GetScreenWidth(); }
Rectangle Box(float dx, float y, float w, float h) { return {SW() / 2 + dx * ui, y * ui, w * ui, h * ui}; }
int FSZ(float f) { return std::max(8, (int)(f * ui + 0.5f)); }

void Text(const char* t, float dx, float y, float fs, Color c) {
    int x = (int)(SW() / 2 + dx * ui), yy = (int)(y * ui), s = FSZ(fs), off = std::max(1, (int)(ui * 1.5f));
    DrawText(t, x + off, yy + off, s, Color{0, 0, 0, 150});
    DrawText(t, x, yy, s, c);
}
void TextC(const char* t, float y, float fs, Color c) {
    int s = FSZ(fs), off = std::max(1, (int)(ui * 1.5f));
    int x = (GetScreenWidth() - MeasureText(t, s)) / 2, yy = (int)(y * ui);
    DrawText(t, x + off, yy + off, s, Color{0, 0, 0, 150});
    DrawText(t, x, yy, s, c);
}
void TextIn(const char* t, Rectangle r, float fs, Color c) {
    int s = FSZ(fs);
    DrawText(t, (int)(r.x + (r.width - MeasureText(t, s)) / 2), (int)(r.y + (r.height - s) / 2), s, c);
}

bool Button(Rectangle r, const char* text, float fs, bool selected = false) {
    static float hv[64] = {0};
    unsigned id = 0;
    for (const char* p = text; *p; p++) id = id * 31 + (unsigned char)*p;
    id %= 64;
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    hv[id] = Lerp(hv[id], hover ? 1.0f : 0.0f, 0.25f);
    float h = hv[id];
    Color base = selected ? Color{40, 120, 80, 235} : Color{28, 44, 100, 225};
    Color hi = selected ? Color{60, 180, 115, 245} : Color{70, 120, 255, 245};
    DrawRectangleRounded(r, 0.25f, 8, Mix(base, hi, h));
    DrawRectangle((int)r.x, (int)(r.y + r.height - 3 * ui), (int)(r.width * h), std::max(1, (int)(2 * ui)), WHITE);
    TextIn(text, r, fs, WHITE);
    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

bool Slider(const void* id, const char* label, float& v, float lo, float hi, float dx, float y, float w, const char* fmt) {
    Rectangle row = Box(dx, y, w, 34);
    Text(label, dx, y + 7, 20, WHITE);
    Rectangle tr = {row.x + row.width * 0.40f, row.y + row.height * 0.5f - 3 * ui, row.width * 0.42f, 6 * ui};
    Rectangle hit = {tr.x - 10 * ui, row.y, tr.width + 20 * ui, row.height};
    Vector2 m = GetMousePosition();
    bool changed = false;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, hit)) activeCtl = id;
    if (activeCtl == id) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float nv = lo + Clamp((m.x - tr.x) / tr.width, 0.0f, 1.0f) * (hi - lo);
            if (nv != v) { v = nv; changed = true; }
        } else activeCtl = nullptr;
    }
    float t = Clamp((v - lo) / (hi - lo), 0.0f, 1.0f);
    DrawRectangleRounded(tr, 1.0f, 6, Color{60, 66, 90, 255});
    DrawRectangleRounded({tr.x, tr.y, std::max(tr.width * t, tr.height), tr.height}, 1.0f, 6, Color{80, 150, 255, 255});
    DrawCircleV({tr.x + tr.width * t, tr.y + tr.height / 2}, 8 * ui, activeCtl == id ? WHITE : Color{210, 225, 255, 255});
    Text(TextFormat(fmt, v), dx + w * 0.85f, y + 7, 20, YELLOW);
    return changed;
}

bool SliderI(const void* id, const char* label, int& v, int lo, int hi, float dx, float y, float w, const char* fmt) {
    float f = (float)v;
    bool ch = Slider(id, label, f, (float)lo, (float)hi, dx, y, w, fmt);
    if (ch) v = std::clamp((int)(f + 0.5f), lo, hi);
    return ch;
}

bool Cycle(const char* label, const char* const* opts, int n, int& cur, float dx, float y, float w, bool enabled = true) {
    Text(label, dx, y + 7, 20, enabled ? WHITE : GRAY);
    Rectangle r = Box(dx + w * 0.40f, y, w * 0.60f, 34);
    bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRounded(r, 0.3f, 8, enabled ? (hover ? Color{70, 120, 255, 235} : Color{36, 56, 130, 225}) : Color{40, 40, 50, 200});
    TextIn(TextFormat("<  %s  >", opts[cur]), r, 20, enabled ? WHITE : GRAY);
    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { cur = (cur + 1) % n; return true; }
    if (hover && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { cur = (cur + n - 1) % n; return true; }
    return false;
}

bool Toggle(const char* label, bool& v, float dx, float y, float w) {
    static const char* const OO[2] = {"OFF", "ON"};
    int c = v ? 1 : 0;
    bool ch = Cycle(label, OO, 2, c, dx, y, w);
    v = c != 0;
    return ch;
}

//  crosshair
void DrawCrosshairCfg(float cx, float cy, const CrosshairCfg& c, float kick, float alphaMul) {
    Color col = c.color < 7 ? CH_COLORS[c.color] : ColorFromHSV(c.hue, 1.0f, 1.0f);
    col.a = (unsigned char)(255.0f * c.opacity / 100.0f * alphaMul);
    Color ol = {0, 0, 0, (unsigned char)(col.a * 0.85f)};
    float g = c.gap + kick, len = (float)c.size, th = (float)c.thickness;
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0 && !c.outline) continue;
        Color cc = pass == 0 ? ol : col;
        float ex = pass == 0 ? 2.0f : 0.0f;
        if (c.style == 0 || c.style == 3) {
            DrawLineEx({cx - g - len - ex / 2, cy}, {cx - g + ex / 2, cy}, th + ex, cc);
            DrawLineEx({cx + g - ex / 2, cy}, {cx + g + len + ex / 2, cy}, th + ex, cc);
            DrawLineEx({cx, cy - g - len - ex / 2}, {cx, cy - g + ex / 2}, th + ex, cc);
            DrawLineEx({cx, cy + g - ex / 2}, {cx, cy + g + len + ex / 2}, th + ex, cc);
        }
        if (c.style == 1 || c.style == 3) DrawCircleV({cx, cy}, std::max(1.5f, th * 0.8f) + ex / 2, cc);
        if (c.style == 2) {
            float R = len + g;
            DrawRing({cx, cy}, R - th / 2 - ex / 2, R + th / 2 + ex / 2, 0, 360, 40, cc);
        }
    }
}

//  game flow
Vector3 Forward() { return {sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch)}; }

void ResetCamera() {
    yaw = pitch = 0;
    camera.position = EYE;
    camera.target = Vector3Add(EYE, Forward());
}

void StartGame(int map, int mode) {
    gameMap = map;
    gameMode = mode;
    settings.lastMap = map;
    targets.clear();
    particles.clear();
    popups.clear();
    stats = {};
    newBest = false;
    hitMarker = crossKick = 0;
    timeLeft = MODES[mode].duration;
    ResetCamera();
    for (int i = 0; i < MODES[mode].count; i++) SpawnTarget(targets, map, MODES[mode].bots);
    countdownValue = 3;
    countdownTimer = 0;
    gameState = COUNTDOWN;
    DisableCursor();
    skipDelta = 2;
}

void PauseGame() { gameState = GAMEPLAY_PAUSED; EnableCursor(); }
void ResumeGame() { gameState = GAMEPLAY; DisableCursor(); skipDelta = 2; }
void GoMenu() { gameState = MENU_MAIN; EnableCursor(); targets.clear(); particles.clear(); popups.clear(); menuIntro = 0; }

void EndGame() {
    gameState = RESULTS;
    EnableCursor();
    newBest = stats.score > bestScore[gameMap][gameMode];
    if (newBest) { bestScore[gameMap][gameMode] = stats.score; SaveScores(); }
}

void UpdateLook() {
    Vector2 d = GetMouseDelta();
    if (skipDelta > 0) { skipDelta--; d = {0, 0}; }
    yaw = Clamp(yaw - d.x * settings.sens * 0.001f, -1.7f, 1.7f);
    pitch = Clamp(pitch - d.y * settings.sens * 0.001f, -1.3f, 1.3f);
    camera.target = Vector3Add(camera.position, Forward());
}

void Shoot() {
    const ModeInfo& m = MODES[gameMode];
    stats.shots++;
    crossKick = 4.0f;
    Play(shootSound);
    Ray ray = {camera.position, Forward()};
    Hit h = CastRay(targets, ray, gameMap, m.reflex);
    if (h.idx < 0) { stats.streak = 0; return; }
    Target t = targets[h.idx];
    int pts = h.zone == 1 ? 2 : 1;
    stats.hits++;
    stats.score += pts;
    stats.streak++;
    stats.bestStreak = std::max(stats.bestStreak, stats.streak);
    stats.killTimeTotal += t.age;
    hitMarker = 0.15f;
    hitMarkerHead = h.zone == 1;
    Vector3 center = TargetCenter(t, gameMap);
    SpawnParticles(particles, center, h.zone == 1 ? ORANGE : TargetColorOf(t));
    popups.push_back({Vector3Add(center, Vector3{0, 0.6f, 0}), 0.8f, pts, h.zone == 1});
    if (audioOk) {
        Sound& s = h.zone == 1 ? headSound : hitSound;
        SetSoundPitch(s, 1.0f + std::min(stats.streak, 10) * 0.04f);
        PlaySound(s);
    }
    targets.erase(targets.begin() + h.idx);
    SpawnTarget(targets, gameMap, m.bots);
}

//  menu scene (animated background)
void UpdateMenuScene(float dt) {
    menuTime += dt;
    if (menuMap != selectedMap) { menuMap = selectedMap; menuTargets.clear(); menuParticles.clear(); demoTarget = -1; }
    for (int i = 0; i < 10 && (int)menuTargets.size() < 7; i++) SpawnTarget(menuTargets, menuMap, false);
    UpdateTargets(menuTargets, dt, true);
    UpdateParticles(menuParticles, dt);

    float my = sinf(menuTime * 0.25f) * 0.3f, mp = 0.05f + sinf(menuTime * 0.17f) * 0.05f;
    Vector3 f = {sinf(my) * cosf(mp), sinf(mp), cosf(my) * cosf(mp)};
    menuCam.position = EYE;
    menuCam.target = Vector3Add(EYE, f);
    menuCam.fovy = 70.0f;

    demoCooldown -= dt;
    if (menuTargets.empty()) return;
    if (demoTarget < 0 || demoTarget >= (int)menuTargets.size()) demoTarget = rand() % (int)menuTargets.size();
    Vector2 sp = GetWorldToScreen(TargetCenter(menuTargets[demoTarget], menuMap), menuCam);
    float k = 1.0f - expf(-dt * 7.0f);
    demoPos = {demoPos.x + (sp.x - demoPos.x) * k, demoPos.y + (sp.y - demoPos.y) * k};
    float dist = hypotf(sp.x - demoPos.x, sp.y - demoPos.y);
    if (dist < 8.0f * ui && demoCooldown <= 0) {
        const Target t = menuTargets[demoTarget];
        SpawnParticles(menuParticles, TargetCenter(t, menuMap), TargetColorOf(t));
        menuTargets.erase(menuTargets.begin() + demoTarget);
        SpawnTarget(menuTargets, menuMap, false);
        demoTarget = -1;
        demoCooldown = 0.35f;
    }
}

void DrawVignette() {
    int w = GetScreenWidth(), h = GetScreenHeight();
    DrawRectangleGradientH(0, 0, w / 5, h, Color{0, 0, 0, 120}, Color{0, 0, 0, 0});
    DrawRectangleGradientH(w - w / 5, 0, w / 5, h, Color{0, 0, 0, 0}, Color{0, 0, 0, 120});
    DrawRectangleGradientV(0, 0, w, h / 6, Color{0, 0, 0, 100}, Color{0, 0, 0, 0});
    DrawRectangleGradientV(0, h - h / 6, w, h / 6, Color{0, 0, 0, 0}, Color{0, 0, 0, 100});
}

void DrawMenuBackground() {
    DrawSky(menuMap);
    BeginMode3D(menuCam);
    DrawMap(menuMap, settings.quality);
    DrawTargets(menuTargets, menuMap, false, settings.quality, menuCam);
    DrawParticles(menuParticles);
    EndMode3D();
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 140}, Color{0, 0, 0, 200});
    if (settings.quality >= Q_MEDIUM) DrawVignette();
    DrawCrosshairCfg(demoPos.x, demoPos.y, settings.ch, 0, 0.8f);
}

void DrawGlowText(const char* text, int x, int y, int fs, float hueOffset) {
    std::string t = text;
    float time = (float)GetTime();
    for (int i = 0; i < (int)t.size(); i++) {
        std::string prefix = t.substr(0, i), ch(1, t[i]);
        int cx = x + MeasureText(prefix.c_str(), fs);
        Color c = ColorFromHSV(fmodf(time * 90.0f + i * 22.0f + hueOffset, 360.0f), 0.85f, 1.0f);
        for (int r = 2; r >= 1; r--) {
            Color g = Fade(c, r == 2 ? 0.07f : 0.14f);
            DrawText(ch.c_str(), cx - r, y, fs, g);
            DrawText(ch.c_str(), cx + r, y, fs, g);
            DrawText(ch.c_str(), cx, y - r, fs, g);
            DrawText(ch.c_str(), cx, y + r, fs, g);
        }
        DrawText(ch.c_str(), cx, y, fs, c);
    }
}

void DrawWatermark() {
    int h = GetScreenHeight();
    int nameFs = FSZ(22), linkFs = FSZ(16), x = (int)(12 * ui);
    DrawGlowText("Habibig04", x, h - (int)(54 * ui), nameFs, 0.0f);
    const char* link = "github.com/Habibig04";
    int lw = MeasureText(link, linkFs);
    Rectangle r = {(float)x, (float)h - 26 * ui, (float)lw, 18 * ui};
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawGlowText(link, x, h - (int)(26 * ui), linkFs, 120.0f);
    if (hover) DrawRectangle(x, h - (int)(8 * ui), lw, 1, WHITE);
    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) OpenURL("https://github.com/Habibig04");
}

void DrawTitle(float y, const char* title, float fsDesign) {
    int fs = FSZ(fsDesign);
    std::string t = title;
    float time = (float)GetTime();
    int x0 = (GetScreenWidth() - MeasureText(title, fs)) / 2;
    for (int i = 0; i < (int)t.size(); i++) {
        std::string prefix = t.substr(0, i), ch(1, t[i]);
        int cx = x0 + MeasureText(prefix.c_str(), fs);
        int cy = (int)(y * ui + sinf(time * 2.2f + i * 0.55f) * 4 * ui);
        Color c = ColorFromHSV(fmodf(time * 50.0f + i * 26.0f, 360.0f), 0.65f, 1.0f);
        for (int r = 2; r >= 1; r--) {
            Color g = Fade(c, 0.12f);
            DrawText(ch.c_str(), cx - r, cy, fs, g);
            DrawText(ch.c_str(), cx + r, cy, fs, g);
            DrawText(ch.c_str(), cx, cy - r, fs, g);
            DrawText(ch.c_str(), cx, cy + r, fs, g);
        }
        DrawText(ch.c_str(), cx + 3, cy + 3, fs, Color{0, 0, 0, 160});
        DrawText(ch.c_str(), cx, cy, fs, c);
    }
}

//  screens
void DrawGameScene() {
    DrawSky(gameMap);
    BeginMode3D(camera);
    DrawMap(gameMap, settings.quality);
    DrawTargets(targets, gameMap, MODES[gameMode].reflex, settings.quality, camera);
    DrawParticles(particles);
    EndMode3D();
    if (settings.quality >= Q_HIGH) DrawVignette();
}

void DrawHUD() {
    const ModeInfo& m = MODES[gameMode];
    int sw = GetScreenWidth();
    float lx = -SW() / 2 / ui + 22;
    DrawRectangleRounded(Box(lx - 12, 10, 230, m.reflex ? 160.0f : 134.0f), 0.12f, 8, Color{0, 0, 0, 120});
    Text(TextFormat("%s - %s", MAPS[gameMap].name, m.name), lx, 16, 16, YELLOW);
    Text(TextFormat("Score: %d", stats.score), lx, 40, 22, WHITE);
    Text(TextFormat("Hits %d / Shots %d", stats.hits, stats.shots), lx, 68, 18, WHITE);
    Text(TextFormat("Accuracy: %.1f%%", stats.accuracy()), lx, 90, 18, WHITE);
    Text(TextFormat("Streak: %d", stats.streak), lx, 112, 18, WHITE);
    if (m.reflex) Text(TextFormat("Missed: %d", stats.expired), lx, 134, 18, WHITE);

    int secs = (int)ceilf(m.duration > 0 ? timeLeft : stats.sessionTime);
    const char* tt = TextFormat("%s %02d:%02d", m.duration > 0 ? "Left" : "Time", secs / 60, secs % 60);
    int tfs = FSZ(24);
    DrawText(tt, sw - MeasureText(tt, tfs) - (int)(20 * ui), (int)(16 * ui), tfs, WHITE);
    const char* tp = TextFormat("TPM %.1f", stats.tpm());
    DrawText(tp, sw - MeasureText(tp, FSZ(18)) - (int)(20 * ui), (int)(48 * ui), FSZ(18), LIGHTGRAY);
    if (settings.showFps) {
        const char* fp = TextFormat("%d FPS", GetFPS());
        DrawText(fp, sw - MeasureText(fp, FSZ(16)) - (int)(20 * ui), (int)(72 * ui), FSZ(16), GRAY);
    }

    for (const Popup& p : popups) {
        float a = p.life / 0.8f;
        Vector2 sp = GetWorldToScreen(Vector3Add(p.pos, Vector3{0, (1.0f - a) * 1.2f, 0}), camera);
        const char* s = p.head ? TextFormat("HEADSHOT +%d", p.pts) : TextFormat("+%d", p.pts);
        int fs = FSZ(p.head ? 24 : 20);
        DrawText(s, (int)sp.x - MeasureText(s, fs) / 2, (int)sp.y, fs, Fade(p.head ? ORANGE : WHITE, a));
    }
    if (toast > 0) TextC(toastMsg, 20, 22, YELLOW);

    float cx = sw / 2.0f, cy = GetScreenHeight() / 2.0f;
    DrawCrosshairCfg(cx, cy, settings.ch, crossKick, 1.0f);
    if (hitMarker > 0) {
        Color c = Fade(hitMarkerHead ? ORANGE : WHITE, hitMarker / 0.15f);
        float a = 8 * ui, b = 16 * ui;
        DrawLineEx({cx - b, cy - b}, {cx - a, cy - a}, 2, c);
        DrawLineEx({cx + b, cy - b}, {cx + a, cy - a}, 2, c);
        DrawLineEx({cx - b, cy + b}, {cx - a, cy + a}, 2, c);
        DrawLineEx({cx + b, cy + b}, {cx + a, cy + a}, 2, c);
    }
}

void DrawMainMenu() {
    DrawTitle(95, "AIM TRAINER", 76);
    TextC("PRECISION TRAINING", 186, 20, Color{190, 210, 255, 255});
    const char* names[3] = {"PLAY", "SETTINGS", "QUIT"};
    for (int i = 0; i < 3; i++) {
        float t = EaseOut(menuIntro * 1.4f - i * 0.25f);
        float off = (1.0f - t) * -520.0f;
        bool clicked = Button(Box(-130 + off, 250.0f + i * 76, 260, 58), names[i], 28);
        if (clicked && t >= 1.0f) {
            if (i == 0) gameState = MENU_SETUP;
            else if (i == 1) { gameState = MENU_SETTINGS; settingsTab = 0; }
            else quitRequested = true;
        }
    }
    TextC("Mouse aim   |   Left click shoot   |   P / Esc pause   |   F11 fullscreen", 640, 18, Color{200, 205, 220, 255});
    const char* gl = GLName();
    DrawText(gl, GetScreenWidth() - MeasureText(gl, FSZ(18)) - (int)(10 * ui), GetScreenHeight() - (int)(26 * ui), FSZ(18), GRAY);
    DrawWatermark();
}

void DrawSetupMenu() {
    TextC("SELECT MAP", 24, 34, YELLOW);
    for (int i = 0; i < NUM_MAPS; i++)
        if (Button(Box(-418.0f + i * 212.0f, 78, 200, 52), MAPS[i].name, 22, selectedMap == i)) selectedMap = i;
    TextC(MAPS[selectedMap].desc, 140, 18, LIGHTGRAY);
    TextC("SELECT MODE", 180, 30, YELLOW);
    for (int i = 0; i < NUM_MODES; i++) {
        int col = i / 4, row = i % 4;
        float dx = col == 0 ? -386.0f : 6.0f, y = 228.0f + row * 66.0f;
        bool clicked = Button(Box(dx, y, 380, 58), MODES[i].name, 22);
        Text(MODES[i].desc, dx + 14, y + 40, 13, Color{190, 200, 230, 255});
        const char* b = TextFormat("Best %d", bestScore[selectedMap][i]);
        DrawText(b, (int)(SW() / 2 + (dx + 372) * ui) - MeasureText(b, FSZ(16)), (int)((y + 6) * ui), FSZ(16), LIGHTGRAY);
        if (clicked) { StartGame(selectedMap, i); return; }
    }
    if (Button(Box(-100, 506, 200, 46), "BACK", 22)) gameState = MENU_MAIN;
}

void DrawSettingsMenu() {
    DrawRectangleRounded(Box(-400, 56, 800, 604), 0.03f, 8, Color{8, 10, 20, 215});
    TextC("SETTINGS", 14, 30, YELLOW);
    for (int i = 0; i < 3; i++)
        if (Button(Box(-312.0f + i * 212.0f, 72, 200, 40), TAB_NAMES[i], 20, settingsTab == i)) settingsTab = i;

    const float dx = -360, w = 720;
    float y = 138;
    if (settingsTab == 0) {
        Slider(&settings.sens, "Mouse sensitivity", settings.sens, 0.1f, 15.0f, dx, y, w, "%.2f"); y += 50;
        Slider(&settings.fov, "Field of view", settings.fov, 50.0f, 120.0f, dx, y, w, "%.0f"); y += 50;
        if (Slider(&settings.volume, "Volume", settings.volume, 0.0f, 100.0f, dx, y, w, "%.0f%%")) SetMasterVolume(settings.volume / 100.0f);
        y += 50;
        Cycle("Target colour", TARGET_COLOR_NAMES, 7, settings.targetColor, dx, y, w);
        Color swatch = settings.targetColor < 6 ? TARGET_COLORS[settings.targetColor] : ColorFromHSV(fmodf((float)GetTime() * 90.0f, 360.0f), 0.8f, 1.0f);
        Rectangle sr = Box(dx + w + 14, y + 4, 26, 26);
        DrawCircle((int)(sr.x + sr.width / 2), (int)(sr.y + sr.height / 2), sr.width / 2, swatch);
        y += 50;
        Toggle("Show FPS", settings.showFps, dx, y, w);
        Text("Tip: right-click a selector to go back one option.", dx, 420, 16, GRAY);
    } else if (settingsTab == 1) {
        Cycle("Graphics quality", QUALITY_NAMES, 3, settings.quality, dx, y, w); y += 50;
        int disp = settings.fullscreen ? 1 : 0;
        if (Cycle("Display mode", DISPLAY_NAMES, 2, disp, dx, y, w)) { settings.fullscreen = disp == 1; ApplyDisplay(); }
        y += 50;
        int res = 1;
        for (int i = 0; i < 5; i++) if (RES_OPTS[i][0] == settings.w && RES_OPTS[i][1] == settings.h) res = i;
        if (Cycle("Window size", RES_NAMES, 5, res, dx, y, w, !settings.fullscreen)) {
            settings.w = RES_OPTS[res][0];
            settings.h = RES_OPTS[res][1];
            SetWindowSize(settings.w, settings.h);
        }
        y += 50;
        if (Cycle("FPS limit", FPS_NAMES, 5, settings.fpsIdx, dx, y, w)) SetTargetFPS(FPS_OPTS[settings.fpsIdx]);
        y += 64;
        const char* desc[3] = {"Potato: lowest detail for slow hardware, no effects.",
                               "Medium: shading, shadows and light effects. Good for laptops and the Pi.",
                               "High: glow, particles, vignette and anti-aliasing (anti-aliasing needs a restart)."};
        for (int i = 2; i >= 0; i--) {
            Text(desc[i], dx, y, 17, settings.quality == i ? WHITE : GRAY);
            y += 26;
        }
        Text("F11 toggles fullscreen anywhere. Fullscreen fits any screen size.", dx, y + 10, 17, Color{190, 210, 255, 255});
    } else {
        CrosshairCfg& c = settings.ch;
        const float cw = 430;
        Cycle("Style", CH_STYLE_NAMES, 4, c.style, dx, y, cw); y += 42;
        Cycle("Colour", CH_COLOR_NAMES, 8, c.color, dx, y, cw); y += 42;
        if (c.color == 7) { Slider(&c.hue, "Hue", c.hue, 0.0f, 360.0f, dx, y, cw, "%.0f"); y += 42; }
        SliderI(&c.size, "Size", c.size, 1, 30, dx, y, cw, "%.0f"); y += 42;
        SliderI(&c.thickness, "Thickness", c.thickness, 1, 8, dx, y, cw, "%.0f"); y += 42;
        SliderI(&c.gap, "Gap", c.gap, 0, 20, dx, y, cw, "%.0f"); y += 42;
        SliderI(&c.opacity, "Opacity", c.opacity, 20, 100, dx, y, cw, "%.0f%%"); y += 42;
        Toggle("Outline", c.outline, dx, y, cw);
        Rectangle pv = Box(110, 138, 250, 250);
        DrawRectangleRounded(pv, 0.06f, 8, Color{22, 26, 40, 255});
        DrawCircle((int)(pv.x + pv.width / 2), (int)(pv.y + pv.height / 2), 22 * ui, TARGET_COLORS[std::min(settings.targetColor, 5)]);
        DrawCrosshairCfg(pv.x + pv.width / 2, pv.y + pv.height / 2, c, 0, 1.0f);
        TextIn("Preview", Rectangle{pv.x, pv.y + pv.height - 30 * ui, pv.width, 24 * ui}, 16, GRAY);
    }
    if (Button(Box(-120, 604, 240, 44), "SAVE & BACK", 22)) { settings.save(); gameState = MENU_MAIN; menuIntro = 0; }
}

void DrawPauseMenu() {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 160});
    TextC("PAUSED", 140, 44, YELLOW);
    if (Button(Box(-140, 220, 280, 52), "RESUME (P)", 24)) ResumeGame();
    else if (Button(Box(-140, 284, 280, 52), "RESTART (R)", 24)) StartGame(gameMap, gameMode);
    else if (Button(Box(-140, 348, 280, 52), "FINISH (F)", 24)) EndGame();
    else if (Button(Box(-140, 412, 280, 52), "MENU (ESC)", 24)) GoMenu();
}

void DrawResults() {
    DrawRectangleRounded(Box(-260, 40, 520, 600), 0.04f, 8, Color{8, 10, 20, 215});
    TextC("SESSION RESULTS", 56, 38, YELLOW);
    TextC(TextFormat("%s  -  %s", MAPS[gameMap].name, MODES[gameMode].name), 106, 22, LIGHTGRAY);
    float x = -200, y = 160;
    Text(TextFormat("Score: %d   (Best %d)", stats.score, bestScore[gameMap][gameMode]), x, y, 26, WHITE); y += 40;
    Text(TextFormat("Targets hit: %d", stats.hits), x, y, 24, WHITE); y += 36;
    Text(TextFormat("Shots fired: %d", stats.shots), x, y, 24, WHITE); y += 36;
    Text(TextFormat("Accuracy: %.1f%%", stats.accuracy()), x, y, 24, WHITE); y += 36;
    Text(TextFormat("Targets / min: %.1f", stats.tpm()), x, y, 24, WHITE); y += 36;
    Text(TextFormat("Avg time to hit: %.2fs", stats.avgKill()), x, y, 24, WHITE); y += 36;
    Text(TextFormat("Best streak: %d", stats.bestStreak), x, y, 24, WHITE); y += 36;
    if (MODES[gameMode].reflex) { Text(TextFormat("Missed: %d", stats.expired), x, y, 24, WHITE); y += 36; }
    if (newBest) TextC("NEW BEST!", y + 6, 32, GOLD);
    if (Button(Box(-230, 560, 220, 52), "RETRY (R)", 24)) StartGame(gameMap, gameMode);
    else if (Button(Box(10, 560, 220, 52), "MENU (SPACE)", 24)) GoMenu();
}

//  main
int main() {
    srand((unsigned)time(nullptr));
    InitMaps();
    bool hadSettings = settings.load();
    int glv = rlGetVersion();
    if (!hadSettings && (glv == RL_OPENGL_ES_20 || glv == RL_OPENGL_21 || glv == RL_OPENGL_11)) settings.quality = Q_MEDIUM;
    selectedMap = menuMap = settings.lastMap;
    LoadScores();

    unsigned flags = FLAG_WINDOW_RESIZABLE;
    if (settings.quality == Q_HIGH) flags |= FLAG_MSAA_4X_HINT;
    SetConfigFlags(flags);
    InitWindow(settings.w, settings.h, "AIM TRAINER");
    if (!IsWindowReady() && (flags & FLAG_MSAA_4X_HINT)) {
        TraceLog(LOG_WARNING, "Anti-aliasing unavailable, retrying without it");
        flags &= ~(unsigned)FLAG_MSAA_4X_HINT;
        SetConfigFlags(flags);
        InitWindow(settings.w, settings.h, "AIM TRAINER");
    }
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "Window/GL context failed. raylib was built for a GL version your GPU can't provide.");
        TraceLog(LOG_ERROR, "Use ./run.sh, or rebuild with -DRAYLIB_GL=\"ES 2.0\" (Pi / ARM) or -DRAYLIB_GL=2.1 (old GPUs / VMs)");
        return 1;
    }
    TraceLog(LOG_INFO, "Renderer backend: %s", GLName());
    SetExitKey(KEY_NULL);
    SetTargetFPS(FPS_OPTS[settings.fpsIdx]);
    InitAudioDevice();
    audioOk = IsAudioDeviceReady();
    if (audioOk) {
        hitSound = MakeBeep(880, 0.08f, 0.5f);
        headSound = MakeBeep(1320, 0.10f, 0.5f);
        shootSound = MakeBeep(220, 0.04f, 0.25f);
        missSound = MakeBeep(140, 0.10f, 0.3f);
        SetMasterVolume(settings.volume / 100.0f);
    }
    camera.up = {0, 1, 0};
    camera.fovy = settings.fov;
    camera.projection = CAMERA_PERSPECTIVE;
    menuCam = camera;
    ResetCamera();
    demoPos = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    if (settings.fullscreen) ApplyDisplay();

    while (!quitRequested && !WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);
        ui = Clamp(GetScreenHeight() / 720.0f, 0.6f, 3.0f);
        camera.fovy = settings.fov;
        if (IsWindowResized() && !fsActive) { settings.w = GetScreenWidth(); settings.h = GetScreenHeight(); }
        if (hitMarker > 0) hitMarker -= dt;
        if (toast > 0) toast -= dt;
        if (focusGrace > 0) focusGrace -= dt;
        if (crossKick > 0) crossKick = std::max(0.0f, crossKick - 30.0f * dt);
        if (IsKeyPressed(KEY_F11)) ToggleFS();

        bool menuState = gameState == MENU_MAIN || gameState == MENU_SETUP || gameState == MENU_SETTINGS || gameState == RESULTS;
        if (menuState) UpdateMenuScene(dt);

        switch (gameState) {
            case MENU_MAIN:
                menuIntro += dt;
                break;
            case MENU_SETUP:
                if (IsKeyPressed(KEY_ESCAPE)) gameState = MENU_MAIN;
                break;
            case MENU_SETTINGS:
                if (IsKeyPressed(KEY_ESCAPE)) { settings.save(); gameState = MENU_MAIN; menuIntro = 0; }
                break;
            case COUNTDOWN:
                UpdateLook();
                countdownTimer += dt;
                if (countdownTimer >= 1.0f) {
                    countdownTimer -= 1.0f;
                    if (--countdownValue <= 0) gameState = GAMEPLAY;
                }
                break;
            case GAMEPLAY: {
                const ModeInfo& m = MODES[gameMode];
                if ((!IsWindowFocused() && focusGrace <= 0) || IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) { PauseGame(); break; }
                stats.sessionTime += dt;
                if (!perfChecked) {
                    float raw = GetFrameTime();
                    perfWarm += raw;
                    if (perfWarm > 1.0f) {
                        perfTime += raw;
                        perfFrames++;
                        if (perfTime >= 4.0f) {
                            perfChecked = true;
                            if (perfFrames / perfTime < 45.0f && settings.quality != Q_POTATO) {
                                settings.quality = Q_POTATO;
                                settings.save();
                                toastMsg = "Low FPS detected: Potato graphics enabled";
                                toast = 4.0f;
                            }
                        }
                    }
                }
                if (m.duration > 0) {
                    timeLeft -= dt;
                    if (timeLeft <= 0) { timeLeft = 0; EndGame(); break; }
                }
                UpdateLook();
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) Shoot();
                UpdateTargets(targets, dt, m.moving);
                if (m.reflex) {
                    for (int i = (int)targets.size() - 1; i >= 0; i--)
                        if (targets[i].age >= REFLEX_LIFETIME) {
                            targets.erase(targets.begin() + i);
                            stats.expired++;
                            stats.streak = 0;
                            Play(missSound);
                            SpawnTarget(targets, gameMap, false);
                        }
                }
                UpdateParticles(particles, dt);
                for (Popup& p : popups) p.life -= dt;
                popups.erase(std::remove_if(popups.begin(), popups.end(), [](const Popup& p) { return p.life <= 0; }), popups.end());
                break;
            }
            case GAMEPLAY_PAUSED:
                if (IsKeyPressed(KEY_P)) ResumeGame();
                else if (IsKeyPressed(KEY_R)) StartGame(gameMap, gameMode);
                else if (IsKeyPressed(KEY_F)) EndGame();
                else if (IsKeyPressed(KEY_ESCAPE)) GoMenu();
                break;
            case RESULTS:
                if (IsKeyPressed(KEY_R)) StartGame(gameMap, gameMode);
                else if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) GoMenu();
                break;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        switch (gameState) {
            case MENU_MAIN: DrawMenuBackground(); DrawMainMenu(); break;
            case MENU_SETUP: DrawMenuBackground(); DrawSetupMenu(); break;
            case MENU_SETTINGS: DrawMenuBackground(); DrawSettingsMenu(); break;
            case COUNTDOWN:
                DrawGameScene();
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 110});
                TextC(TextFormat("%d", countdownValue), GetScreenHeight() / 2.0f / ui - 50, 90, YELLOW);
                DrawCrosshairCfg(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f, settings.ch, 0, 1.0f);
                break;
            case GAMEPLAY: DrawGameScene(); DrawHUD(); break;
            case GAMEPLAY_PAUSED: DrawGameScene(); DrawHUD(); DrawPauseMenu(); break;
            case RESULTS: DrawMenuBackground(); DrawResults(); break;
        }
        EndDrawing();
    }

    settings.save();
    if (audioOk) {
        UnloadSound(hitSound);
        UnloadSound(headSound);
        UnloadSound(shootSound);
        UnloadSound(missSound);
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
