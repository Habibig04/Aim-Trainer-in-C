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

// ---------------------------------------------------------------- constants
constexpr float ARENA_W = 40.0f;
constexpr float ARENA_H = 20.0f;
constexpr float ARENA_L = 60.0f;
constexpr float TARGET_R = 0.5f;
constexpr float SPAWN_MARGIN = 2.0f;
constexpr float MIN_TARGET_GAP = 3.0f;
constexpr float MIN_SPAWN_DIST = 8.0f;
constexpr float REFLEX_LIFETIME = 1.5f;
constexpr float MOVE_SPEED = 15.0f;
constexpr int MAX_TARGETS = 20;
constexpr int NUM_MODES = 7;

const Color COL_DARKRED = {139, 0, 0, 255};
const Color COL_CROSS = {0, 255, 120, 255};

enum GameState { MENU_MAIN, MENU_MODES, MENU_SETTINGS, COUNTDOWN, GAMEPLAY, GAMEPLAY_PAUSED, RESULTS };
enum GameMode { MODE_STATIC, MODE_MOVING, MODE_REFLEX, MODE_TIMED_30, MODE_TIMED_60, MODE_TIMED_120, MODE_ENDLESS };

struct ModeInfo {
    const char* name;
    const char* desc;
    float duration;  // 0 = untimed
    int count;       // targets on screen
    bool moving;
    bool reflex;     // targets shrink and expire
};

const ModeInfo MODES[NUM_MODES] = {
    {"Static",      "Stationary targets",       0,   10, false, false},
    {"Moving",      "Targets strafe around",    0,   10, true,  false},
    {"Reflex",      "Targets shrink and vanish", 0,  3,  false, true},
    {"30 Seconds",  "Static sprint",            30,  10, false, false},
    {"60 Seconds",  "Static, one minute",       60,  10, false, false},
    {"120 Seconds", "Static, two minutes",      120, 10, false, false},
    {"Endless",     "Moving targets, no limit", 0,   10, true,  false},
};

const int RESOLUTIONS[][2] = {{1024, 576}, {1280, 720}, {1600, 900}, {1920, 1080}};
constexpr int NUM_RES = 4;

// ---------------------------------------------------------------- data types
struct Settings {
    float sens = 0.003f;
    float fov = 60.0f;
    int w = 1280, h = 720;
    bool fullscreen = false;
    bool potato = false;

    void sanitize() {
        sens = Clamp(sens, 0.0005f, 0.02f);
        fov = Clamp(fov, 40.0f, 110.0f);
        if (w < 640 || h < 360 || w > 7680 || h > 4320) { w = 1280; h = 720; }
    }
    void save() const {
        std::ofstream f("settings.cfg");
        if (f) f << sens << "\n" << fov << "\n" << w << "\n" << h << "\n" << fullscreen << "\n" << potato << "\n";
    }
    void load() {
        std::ifstream f("settings.cfg");
        int fs = 0, po = 0;
        if (f >> sens >> fov >> w >> h >> fs >> po) { fullscreen = fs; potato = po; }
        sanitize();
    }
};

struct Target {
    Vector3 pos{0, 0, 0};
    Vector3 vel{0, 0, 0};
    float age = 0, moveTimer = 0, moveInterval = 2, speed = 5;
    bool humanoid = false;
};

struct Particle {
    Vector3 pos, vel;
    float life;
    Color col;
};

struct GameStats {
    int hits = 0, shots = 0, expired = 0, score = 0, streak = 0, bestStreak = 0;
    float sessionTime = 0, killTimeTotal = 0;
    float accuracy() const { return shots > 0 ? 100.0f * hits / shots : 0.0f; }
    float tpm() const { return sessionTime > 0.1f ? hits / sessionTime * 60.0f : 0.0f; }
    float avgKill() const { return hits > 0 ? killTimeTotal / hits : 0.0f; }
};

// ---------------------------------------------------------------- globals
GameState gameState = MENU_MAIN;
GameMode gameMode = MODE_ENDLESS;
Settings settings;
Camera3D camera;
std::vector<Target> targets;
std::vector<Particle> particles;
GameStats stats;
int bestScore[NUM_MODES] = {0};
bool newBest = false;
bool quitRequested = false;

float yaw = 0, pitch = 0;
int skipDelta = 0;
float timeLeft = 0;
int countdownValue = 3;
float countdownTimer = 0;
float hitMarker = 0;
bool hitMarkerHead = false;

bool audioOk = false;
Sound hitSound, headSound, shootSound, missSound;

// ---------------------------------------------------------------- helpers
float RandF(float a, float b) { return a + (float)rand() / (float)RAND_MAX * (b - a); }

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

const char* GLName() {
    switch (rlGetVersion()) {
        case RL_OPENGL_11: return "OpenGL 1.1";
        case RL_OPENGL_21: return "OpenGL 2.1";
        case RL_OPENGL_33: return "OpenGL 3.3";
        case RL_OPENGL_43: return "OpenGL 4.3";
        case RL_OPENGL_ES_20: return "OpenGL ES 2.0";
        case RL_OPENGL_ES_30: return "OpenGL ES 3.0";
        default: return "OpenGL (unknown)";
    }
}

void Play(Sound& s) { if (audioOk) PlaySound(s); }

void LoadScores() {
    std::ifstream f("scores.cfg");
    for (int i = 0; i < NUM_MODES; i++) if (!(f >> bestScore[i])) bestScore[i] = 0;
}
void SaveScores() {
    std::ofstream f("scores.cfg");
    if (f) for (int i = 0; i < NUM_MODES; i++) f << bestScore[i] << "\n";
}

Vector3 Forward() { return {sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch)}; }

float TargetRadius(const Target& t) {
    if (MODES[gameMode].reflex) {
        float k = Clamp(1.0f - t.age / REFLEX_LIFETIME, 0.0f, 1.0f);
        return TARGET_R * (0.35f + 0.65f * k);
    }
    return TARGET_R;
}

void RandomizeVelocity(Target& t) {
    float a = RandF(0, 2 * PI);
    t.vel = {cosf(a) * t.speed, RandF(-0.3f, 0.3f) * t.speed, sinf(a) * t.speed};
}

// ---------------------------------------------------------------- targets
void SpawnTarget() {
    if ((int)targets.size() >= MAX_TARGETS) return;
    const ModeInfo& m = MODES[gameMode];
    Target t;
    for (int tries = 0; tries < 50; tries++) {
        t.pos = {RandF(SPAWN_MARGIN, ARENA_W - SPAWN_MARGIN), RandF(2.0f, ARENA_H - 2.0f), RandF(SPAWN_MARGIN, ARENA_L - SPAWN_MARGIN)};
        bool ok = Vector3Distance(t.pos, camera.position) >= MIN_SPAWN_DIST;
        for (const Target& o : targets)
            if (Vector3Distance(t.pos, o.pos) < MIN_TARGET_GAP) { ok = false; break; }
        if (ok) break;
    }
    t.speed = RandF(2.0f, 7.0f);
    t.moveInterval = RandF(1.0f, 4.0f);
    RandomizeVelocity(t);
    t.humanoid = !m.reflex && rand() % 3 == 0;
    targets.push_back(t);
}

void UpdateTargets(float dt) {
    const ModeInfo& m = MODES[gameMode];
    const float M = 1.2f;
    for (Target& t : targets) {
        t.age += dt;
        if (!m.moving) continue;
        t.moveTimer += dt;
        if (t.moveTimer >= t.moveInterval) { RandomizeVelocity(t); t.moveTimer = 0; }
        t.pos = Vector3Add(t.pos, Vector3Scale(t.vel, dt));
        if (t.pos.x < M || t.pos.x > ARENA_W - M) { t.vel.x = -t.vel.x; t.pos.x = Clamp(t.pos.x, M, ARENA_W - M); }
        if (t.pos.y < M || t.pos.y > ARENA_H - M) { t.vel.y = -t.vel.y; t.pos.y = Clamp(t.pos.y, M, ARENA_H - M); }
        if (t.pos.z < M || t.pos.z > ARENA_L - M) { t.vel.z = -t.vel.z; t.pos.z = Clamp(t.pos.z, M, ARENA_L - M); }
    }
}

struct HitResult { int index = -1; bool head = false; };

HitResult CastRay(Ray ray) {
    HitResult best;
    float closest = FLT_MAX;
    for (int i = 0; i < (int)targets.size(); i++) {
        const Target& t = targets[i];
        if (t.humanoid) {
            RayCollision h = GetRayCollisionSphere(ray, {t.pos.x, t.pos.y + 0.9f, t.pos.z}, 0.3f);
            if (h.hit && h.distance < closest) { closest = h.distance; best = {i, true}; }
            BoundingBox bb = {{t.pos.x - 0.3f, t.pos.y - 0.6f, t.pos.z - 0.3f}, {t.pos.x + 0.3f, t.pos.y + 0.6f, t.pos.z + 0.3f}};
            RayCollision b = GetRayCollisionBox(ray, bb);
            if (b.hit && b.distance < closest) { closest = b.distance; best = {i, false}; }
        } else {
            RayCollision c = GetRayCollisionSphere(ray, t.pos, TargetRadius(t));
            if (c.hit && c.distance < closest) { closest = c.distance; best = {i, false}; }
        }
    }
    return best;
}

void SpawnParticles(Vector3 p, Color c) {
    if (settings.potato) return;
    for (int i = 0; i < 14; i++) {
        Vector3 d = Vector3Normalize({RandF(-1, 1), RandF(-1, 1), RandF(-1, 1)});
        particles.push_back({p, Vector3Scale(d, RandF(2, 6)), 0.5f, c});
    }
}

void UpdateParticles(float dt) {
    for (Particle& p : particles) {
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        p.vel.y -= 9.0f * dt;
        p.life -= dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.life <= 0; }), particles.end());
}

// ---------------------------------------------------------------- game flow
void ResetCamera() {
    yaw = pitch = 0;
    camera.position = {ARENA_W / 2, 4.0f, 3.0f};
    camera.target = Vector3Add(camera.position, Forward());
}

void StartGame(GameMode m) {
    gameMode = m;
    targets.clear();
    particles.clear();
    stats = {};
    newBest = false;
    hitMarker = 0;
    timeLeft = MODES[m].duration;
    ResetCamera();
    for (int i = 0; i < MODES[m].count; i++) SpawnTarget();
    countdownValue = 3;
    countdownTimer = 0;
    gameState = COUNTDOWN;
    DisableCursor();
    skipDelta = 2;
}

void PauseGame() { gameState = GAMEPLAY_PAUSED; EnableCursor(); }
void ResumeGame() { gameState = GAMEPLAY; DisableCursor(); skipDelta = 2; }
void GoMenu() { gameState = MENU_MAIN; EnableCursor(); targets.clear(); particles.clear(); }

void EndGame() {
    gameState = RESULTS;
    EnableCursor();
    newBest = stats.score > bestScore[gameMode];
    if (newBest) { bestScore[gameMode] = stats.score; SaveScores(); }
}

void UpdateLook() {
    Vector2 d = GetMouseDelta();
    if (skipDelta > 0) { skipDelta--; d = {0, 0}; }
    yaw -= d.x * settings.sens;
    pitch = Clamp(pitch - d.y * settings.sens, -1.5f, 1.5f);
    camera.target = Vector3Add(camera.position, Forward());
}

void UpdateMove(float dt) {
    Vector3 f = {sinf(yaw), 0, cosf(yaw)};
    Vector3 r = Vector3CrossProduct(f, camera.up);
    Vector3 m = {0, 0, 0};
    if (IsKeyDown(KEY_W)) m = Vector3Add(m, f);
    if (IsKeyDown(KEY_S)) m = Vector3Subtract(m, f);
    if (IsKeyDown(KEY_D)) m = Vector3Add(m, r);
    if (IsKeyDown(KEY_A)) m = Vector3Subtract(m, r);
    if (IsKeyDown(KEY_SPACE)) m.y += 1;
    if (IsKeyDown(KEY_LEFT_SHIFT)) m.y -= 1;
    if (Vector3Length(m) > 0) {
        camera.position = Vector3Add(camera.position, Vector3Scale(Vector3Normalize(m), MOVE_SPEED * dt));
        camera.position.x = Clamp(camera.position.x, 1.0f, ARENA_W - 1.0f);
        camera.position.y = Clamp(camera.position.y, 1.0f, ARENA_H - 1.0f);
        camera.position.z = Clamp(camera.position.z, 1.0f, ARENA_L - 1.0f);
    }
}

void Shoot() {
    stats.shots++;
    Play(shootSound);
    Ray ray = {camera.position, Forward()};
    HitResult h = CastRay(ray);
    if (h.index < 0) {
        stats.streak = 0;
        return;
    }
    Target t = targets[h.index];
    stats.hits++;
    stats.score += h.head ? 2 : 1;
    stats.streak++;
    stats.bestStreak = std::max(stats.bestStreak, stats.streak);
    stats.killTimeTotal += t.age;
    hitMarker = 0.15f;
    hitMarkerHead = h.head;
    SpawnParticles(t.pos, h.head ? ORANGE : RED);
    if (audioOk) {
        Sound& s = h.head ? headSound : hitSound;
        SetSoundPitch(s, 1.0f + std::min(stats.streak, 10) * 0.04f);
        PlaySound(s);
    }
    targets.erase(targets.begin() + h.index);
    SpawnTarget();
}

// ---------------------------------------------------------------- drawing
void DrawCentered(const char* t, int y, int fs, Color c) {
    DrawText(t, (GetScreenWidth() - MeasureText(t, fs)) / 2, y, fs, c);
}

bool Button(float cx, float y, float w, float h, const char* text, int fs) {
    Rectangle r = {cx - w / 2, y, w, h};
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hover ? Color{70, 130, 255, 255} : Color{40, 80, 200, 255});
    DrawRectangleLinesEx(r, 2, WHITE);
    DrawText(text, (int)(cx - MeasureText(text, fs) / 2), (int)(y + (h - fs) / 2), fs, WHITE);
    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void DrawArena(bool potato) {
    DrawCube({ARENA_W / 2, -0.5f, ARENA_L / 2}, ARENA_W, 1, ARENA_L, DARKGRAY);
    DrawCube({ARENA_W / 2, ARENA_H + 0.5f, ARENA_L / 2}, ARENA_W, 1, ARENA_L, Color{40, 40, 50, 255});
    DrawCube({-0.5f, ARENA_H / 2, ARENA_L / 2}, 1, ARENA_H, ARENA_L, DARKBLUE);
    DrawCube({ARENA_W + 0.5f, ARENA_H / 2, ARENA_L / 2}, 1, ARENA_H, ARENA_L, DARKBLUE);
    DrawCube({ARENA_W / 2, ARENA_H / 2, -0.5f}, ARENA_W, ARENA_H, 1, DARKBLUE);
    DrawCube({ARENA_W / 2, ARENA_H / 2, ARENA_L + 0.5f}, ARENA_W, ARENA_H, 1, DARKBLUE);
    if (potato) return;
    Color g = {120, 120, 150, 255};
    const float e = 0.02f;
    for (float x = 0; x <= ARENA_W; x += 4) {
        DrawLine3D({x, e, 0}, {x, e, ARENA_L}, g);
        DrawLine3D({x, 0, ARENA_L - e}, {x, ARENA_H, ARENA_L - e}, g);
    }
    for (float z = 0; z <= ARENA_L; z += 4) {
        DrawLine3D({0, e, z}, {ARENA_W, e, z}, g);
        DrawLine3D({e, 0, z}, {e, ARENA_H, z}, g);
        DrawLine3D({ARENA_W - e, 0, z}, {ARENA_W - e, ARENA_H, z}, g);
    }
    for (float y = 0; y <= ARENA_H; y += 4) {
        DrawLine3D({0, y, ARENA_L - e}, {ARENA_W, y, ARENA_L - e}, g);
        DrawLine3D({e, y, 0}, {e, y, ARENA_L}, g);
        DrawLine3D({ARENA_W - e, y, 0}, {ARENA_W - e, y, ARENA_L}, g);
    }
}

void DrawTargets(bool potato) {
    bool reflex = MODES[gameMode].reflex;
    for (const Target& t : targets) {
        Color c = RED;
        if (reflex) {
            float k = Clamp(t.age / REFLEX_LIFETIME, 0.0f, 1.0f);
            c = {255, (unsigned char)(220 * (1.0f - k)), 40, 255};
        }
        if (t.humanoid && !potato) {
            DrawCube(t.pos, 0.6f, 1.2f, 0.6f, c);
            DrawSphere({t.pos.x, t.pos.y + 0.9f, t.pos.z}, 0.3f, ORANGE);
        } else {
            float r = TargetRadius(t);
            DrawSphere(t.pos, r, c);
            if (!potato) DrawSphereWires(t.pos, r, 8, 8, COL_DARKRED);
        }
    }
}

void DrawParticles() {
    for (const Particle& p : particles) DrawCube(p.pos, 0.12f, 0.12f, 0.12f, Fade(p.col, p.life / 0.5f));
}

void DrawScene() {
    BeginMode3D(camera);
    DrawArena(settings.potato);
    DrawTargets(settings.potato);
    DrawParticles();
    EndMode3D();
}

void DrawCrosshair() {
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2, gap = 5, len = 10;
    DrawLineEx({(float)(cx - gap - len), (float)cy}, {(float)(cx - gap), (float)cy}, 2, COL_CROSS);
    DrawLineEx({(float)(cx + gap), (float)cy}, {(float)(cx + gap + len), (float)cy}, 2, COL_CROSS);
    DrawLineEx({(float)cx, (float)(cy - gap - len)}, {(float)cx, (float)(cy - gap)}, 2, COL_CROSS);
    DrawLineEx({(float)cx, (float)(cy + gap)}, {(float)cx, (float)(cy + gap + len)}, 2, COL_CROSS);
    if (hitMarker > 0) {
        Color c = Fade(hitMarkerHead ? ORANGE : WHITE, hitMarker / 0.15f);
        int a = 8, b = 16;
        DrawLineEx({(float)(cx - b), (float)(cy - b)}, {(float)(cx - a), (float)(cy - a)}, 2, c);
        DrawLineEx({(float)(cx + b), (float)(cy - b)}, {(float)(cx + a), (float)(cy - a)}, 2, c);
        DrawLineEx({(float)(cx - b), (float)(cy + b)}, {(float)(cx - a), (float)(cy + a)}, 2, c);
        DrawLineEx({(float)(cx + b), (float)(cy + b)}, {(float)(cx + a), (float)(cy + a)}, 2, c);
    }
}

void DrawHUD() {
    const ModeInfo& m = MODES[gameMode];
    int sw = GetScreenWidth();
    DrawFPS(10, 10);
    DrawText(m.name, 10, 36, 20, YELLOW);
    DrawText(TextFormat("Score: %d", stats.score), 10, 62, 20, WHITE);
    DrawText(TextFormat("Hits: %d | Shots: %d", stats.hits, stats.shots), 10, 88, 20, WHITE);
    DrawText(TextFormat("Accuracy: %.1f%%", stats.accuracy()), 10, 114, 20, WHITE);
    DrawText(TextFormat("Streak: %d", stats.streak), 10, 140, 20, WHITE);
    if (m.reflex) DrawText(TextFormat("Missed: %d", stats.expired), 10, 166, 20, WHITE);

    int secs = (int)ceilf(m.duration > 0 ? timeLeft : stats.sessionTime);
    DrawText(TextFormat("%s %02d:%02d", m.duration > 0 ? "Left" : "Time", secs / 60, secs % 60), sw - 170, 36, 20, WHITE);
    DrawText(TextFormat("TPM: %.1f", stats.tpm()), sw - 170, 62, 20, WHITE);
    DrawCrosshair();
}

void DrawGlowText(const char* text, int x, int y, int fs, float hueOffset) {
    std::string t = text;
    float time = (float)GetTime();
    for (int i = 0; i < (int)t.size(); i++) {
        std::string prefix = t.substr(0, i);
        std::string ch(1, t[i]);
        int cx = x + MeasureText(prefix.c_str(), fs);
        Color c = ColorFromHSV(fmodf(time * 90.0f + i * 22.0f + hueOffset, 360.0f), 0.85f, 1.0f);
        for (int r = 3; r >= 1; r--) {
            float a = 0.10f * (4 - r);
            for (int dx = -r; dx <= r; dx += r) {
                for (int dy = -r; dy <= r; dy += r) {
                    if (dx || dy) DrawText(ch.c_str(), cx + dx, y + dy, fs, Fade(c, a));
                }
            }
        }
        DrawText(ch.c_str(), cx, y, fs, c);
    }
}

void DrawWatermark() {
    const char* name = "Habibig04";
    const char* link = "github.com/Habibig04";
    int h = GetScreenHeight();
    DrawGlowText(name, 12, h - 62, 28, 0.0f);
    int lw = MeasureText(link, 18);
    Rectangle r = {12, (float)(h - 28), (float)lw, 20};
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawGlowText(link, 12, h - 28, 18, 120.0f);
    if (hover) DrawRectangle(12, h - 8, lw, 2, WHITE);
    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) OpenURL("https://github.com/Habibig04");
}

void DrawMainMenu() {
    float cx = GetScreenWidth() / 2.0f;
    DrawCentered("AIM TRAINER", 80, 50, YELLOW);
    if (Button(cx, 200, 240, 60, "PLAY", 30)) gameState = MENU_MODES;
    if (Button(cx, 280, 240, 60, "SETTINGS", 30)) gameState = MENU_SETTINGS;
    if (Button(cx, 360, 240, 60, "QUIT", 30)) quitRequested = true;
    DrawCentered("WASD move | Space/Shift up/down | LMB shoot | P pause", 480, 20, LIGHTGRAY);
    DrawCentered("Orange head = 2 points", 510, 20, LIGHTGRAY);
    DrawText(GLName(), GetScreenWidth() - MeasureText(GLName(), 18) - 10, GetScreenHeight() - 26, 18, GRAY);
    DrawWatermark();
}

void DrawModeSelection() {
    float cx = GetScreenWidth() / 2.0f;
    DrawCentered("SELECT GAME MODE", 30, 40, YELLOW);
    for (int i = 0; i < NUM_MODES; i++) {
        float y = 100.0f + i * 56.0f;
        if (Button(cx, y, 420, 46, TextFormat("%s - %s", MODES[i].name, MODES[i].desc), 20)) {
            StartGame((GameMode)i);
            return;
        }
        DrawText(TextFormat("Best: %d", bestScore[i]), (int)cx + 225, (int)y + 13, 20, LIGHTGRAY);
    }
    if (Button(cx, 100.0f + NUM_MODES * 56.0f + 8, 200, 44, "BACK", 22)) gameState = MENU_MAIN;
}

void DrawSettingsMenu() {
    DrawCentered("SETTINGS", 40, 40, YELLOW);
    DrawText(TextFormat("Sensitivity: %.2f  (Left / Right)", settings.sens * 1000.0f), 50, 120, 20, WHITE);
    DrawText(TextFormat("FOV: %.0f  (Up / Down)", settings.fov), 50, 160, 20, WHITE);
    DrawText(TextFormat("Resolution: %dx%d  (R to cycle)", settings.w, settings.h), 50, 200, 20, WHITE);
    DrawText(TextFormat("Fullscreen: %s  (F)", settings.fullscreen ? "ON" : "OFF"), 50, 240, 20, WHITE);
    DrawText(TextFormat("Potato Mode: %s  (P)", settings.potato ? "ON" : "OFF"), 50, 280, 20, WHITE);
    DrawText("Press ESC to save and return", 50, 360, 20, LIGHTGRAY);
}

void DrawPauseMenu() {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 160});
    float cx = GetScreenWidth() / 2.0f;
    float cy = GetScreenHeight() / 2.0f;
    DrawCentered("PAUSED", (int)cy - 170, 40, YELLOW);
    if (Button(cx, cy - 100, 260, 50, "RESUME (P)", 24)) ResumeGame();
    else if (Button(cx, cy - 40, 260, 50, "RESTART (R)", 24)) StartGame(gameMode);
    else if (Button(cx, cy + 20, 260, 50, "FINISH (F)", 24)) EndGame();
    else if (Button(cx, cy + 80, 260, 50, "MENU (ESC)", 24)) GoMenu();
}

void DrawResults() {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 200});
    float cx = GetScreenWidth() / 2.0f;
    DrawCentered("SESSION RESULTS", 40, 40, YELLOW);
    DrawCentered(MODES[gameMode].name, 90, 24, LIGHTGRAY);
    int y = 140, x = (int)cx - 170;
    DrawText(TextFormat("Score: %d   (Best: %d)", stats.score, bestScore[gameMode]), x, y, 25, WHITE);
    DrawText(TextFormat("Targets Hit: %d", stats.hits), x, y + 35, 25, WHITE);
    DrawText(TextFormat("Shots Fired: %d", stats.shots), x, y + 70, 25, WHITE);
    DrawText(TextFormat("Accuracy: %.1f%%", stats.accuracy()), x, y + 105, 25, WHITE);
    DrawText(TextFormat("Targets / Min: %.1f", stats.tpm()), x, y + 140, 25, WHITE);
    DrawText(TextFormat("Avg Time To Hit: %.2fs", stats.avgKill()), x, y + 175, 25, WHITE);
    DrawText(TextFormat("Best Streak: %d", stats.bestStreak), x, y + 210, 25, WHITE);
    if (MODES[gameMode].reflex) DrawText(TextFormat("Missed: %d", stats.expired), x, y + 245, 25, WHITE);
    if (newBest) DrawCentered("NEW BEST!", y + 285, 30, GOLD);
    float by = (float)(y + 335);
    if (Button(cx - 110, by, 200, 50, "RETRY (R)", 24)) StartGame(gameMode);
    else if (Button(cx + 110, by, 200, 50, "MENU (SPACE)", 24)) GoMenu();
}

// ---------------------------------------------------------------- main
int main() {
    srand((unsigned)time(nullptr));
    settings.load();
    LoadScores();

    unsigned flags = 0;
    if (settings.fullscreen) flags |= FLAG_FULLSCREEN_MODE;
    SetConfigFlags(flags);
    InitWindow(settings.w, settings.h, "AIM TRAINER");
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "Window/GL context failed. raylib was built for a GL version your GPU can't provide.");
        TraceLog(LOG_ERROR, "Rebuild with: cmake -DRAYLIB_GL=\"ES 2.0\" (Pi / ARM) or -DRAYLIB_GL=2.1 (old GPUs / VMs)");
        return 1;
    }
    TraceLog(LOG_INFO, "Renderer backend: %s", GLName());
    SetExitKey(KEY_NULL);
    SetTargetFPS(120);

    InitAudioDevice();
    audioOk = IsAudioDeviceReady();
    if (audioOk) {
        hitSound = MakeBeep(880, 0.08f, 0.5f);
        headSound = MakeBeep(1320, 0.10f, 0.5f);
        shootSound = MakeBeep(220, 0.04f, 0.25f);
        missSound = MakeBeep(140, 0.10f, 0.3f);
    }

    camera.up = {0, 1, 0};
    camera.fovy = settings.fov;
    camera.projection = CAMERA_PERSPECTIVE;
    ResetCamera();

    while (!quitRequested && !WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);
        camera.fovy = settings.fov;
        if (hitMarker > 0) hitMarker -= dt;

        // ---------------- update
        switch (gameState) {
            case MENU_MODES:
                if (IsKeyPressed(KEY_ESCAPE)) gameState = MENU_MAIN;
                break;

            case MENU_SETTINGS:
                settings.sens += ((IsKeyDown(KEY_RIGHT) ? 1 : 0) - (IsKeyDown(KEY_LEFT) ? 1 : 0)) * 0.003f * dt;
                settings.fov += ((IsKeyDown(KEY_UP) ? 1 : 0) - (IsKeyDown(KEY_DOWN) ? 1 : 0)) * 30.0f * dt;
                if (IsKeyPressed(KEY_R)) {
                    int idx = 0;
                    for (int i = 0; i < NUM_RES; i++)
                        if (RESOLUTIONS[i][0] == settings.w && RESOLUTIONS[i][1] == settings.h) idx = i + 1;
                    idx %= NUM_RES;
                    settings.w = RESOLUTIONS[idx][0];
                    settings.h = RESOLUTIONS[idx][1];
                    if (!IsWindowFullscreen()) SetWindowSize(settings.w, settings.h);
                }
                if (IsKeyPressed(KEY_F)) {
                    settings.fullscreen = !settings.fullscreen;
                    ToggleFullscreen();
                    if (!settings.fullscreen) SetWindowSize(settings.w, settings.h);
                }
                if (IsKeyPressed(KEY_P)) settings.potato = !settings.potato;
                settings.sanitize();
                if (IsKeyPressed(KEY_ESCAPE)) { settings.save(); gameState = MENU_MAIN; }
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
                if (!IsWindowFocused() || IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) { PauseGame(); break; }
                stats.sessionTime += dt;
                if (MODES[gameMode].duration > 0) {
                    timeLeft -= dt;
                    if (timeLeft <= 0) { timeLeft = 0; EndGame(); break; }
                }
                UpdateMove(dt);
                UpdateLook();
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) Shoot();
                UpdateTargets(dt);
                if (MODES[gameMode].reflex) {
                    for (int i = (int)targets.size() - 1; i >= 0; i--) {
                        if (targets[i].age >= REFLEX_LIFETIME) {
                            targets.erase(targets.begin() + i);
                            stats.expired++;
                            stats.streak = 0;
                            Play(missSound);
                            SpawnTarget();
                        }
                    }
                }
                UpdateParticles(dt);
                break;
            }

            case GAMEPLAY_PAUSED:
                if (IsKeyPressed(KEY_P)) ResumeGame();
                else if (IsKeyPressed(KEY_R)) StartGame(gameMode);
                else if (IsKeyPressed(KEY_F)) EndGame();
                else if (IsKeyPressed(KEY_ESCAPE)) GoMenu();
                break;

            case RESULTS:
                if (IsKeyPressed(KEY_R)) StartGame(gameMode);
                else if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) GoMenu();
                break;

            default:
                break;
        }

        // ---------------- draw (menu buttons are immediate-mode and handle clicks here)
        BeginDrawing();
        ClearBackground(BLACK);
        switch (gameState) {
            case MENU_MAIN: DrawMainMenu(); break;
            case MENU_MODES: DrawModeSelection(); break;
            case MENU_SETTINGS: DrawSettingsMenu(); break;
            case COUNTDOWN:
                DrawScene();
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 120});
                DrawCentered(TextFormat("%d", countdownValue), GetScreenHeight() / 2 - 40, 80, YELLOW);
                break;
            case GAMEPLAY: DrawScene(); DrawHUD(); break;
            case GAMEPLAY_PAUSED: DrawScene(); DrawHUD(); DrawPauseMenu(); break;
            case RESULTS: DrawResults(); break;
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