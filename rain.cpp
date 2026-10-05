#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <ctime>
#include <cmath>
#include <string>
#include <vector>
#include <atomic>
#include <random>
#include <unistd.h>
#include <sys/ioctl.h>

#define MINIAUDIO_IMPLEMENTATION
#include "audio.h"

// ---- colors ----
static const char* RESET      = "\033[0m";
static const char* BLUE_BRIGHT= "\033[38;5;39m";
static const char* BLUE_MID   = "\033[38;5;33m";
static const char* BLUE_DIM   = "\033[38;5;24m";
static const char* BROWN      = "\033[38;5;94m";
static const char* BROWN_DARK = "\033[38;5;58m";
static const char* SPLASH_COL = "\033[38;5;45m";
static const char* HUT_WALL   = "\033[38;5;136m";
static const char* HUT_ROOF   = "\033[38;5;88m";
static const char* HUT_DOOR   = "\033[38;5;52m";
static const char* CHIMNEY_COL= "\033[38;5;240m";
static const char* SMOKE1     = "\033[38;5;252m"; // fresh, brightest
static const char* SMOKE2     = "\033[38;5;245m";
static const char* SMOKE3     = "\033[38;5;238m"; // faint, about to vanish
static const char* FLOWER_STEM = "\033[38;5;34m";
static const char* FLOWER_BLOOM= "\033[38;5;196m";

static const int GROUND_ROWS = 2;
static const int FRAME_US    = 45000;

static std::atomic<bool> g_running{true};
static void on_signal(int) { g_running.store(false); }

// ================= Visual =================
struct Drop { double x, y, speed; int len; };
struct Splash { int x, y, life; };
struct Smoke { double x, y, vx; int life, maxLife; };
struct Flower { int x, height, maxHeight, growTimer; };

static void term_size(int& w, int& h) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) { w = ws.ws_col; h = ws.ws_row; }
    else { w = 80; h = 24; }
}

static void visual_loop() {
    int W, H; term_size(W, H);
    std::mt19937 rng((unsigned)time(nullptr));
    auto rnd = [&rng](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };

    int skyRows = H - GROUND_ROWS;
    if (skyRows < 3) { fprintf(stderr, "Terminal too small.\n"); g_running = false; return; }

    int dropCount = W / 3; if (dropCount < 8) dropCount = 8;
    std::vector<Drop> drops(dropCount);
    for (auto& d : drops) { d.x = rnd(0, W-1); d.y = rnd(-skyRows, skyRows); d.speed = rnd(0.5, 1.6); d.len = 2 + (int)rnd(0,4); }
    std::vector<Splash> splashes;
    std::vector<Smoke> smoke;
    int smokeTimer = 0;
    std::vector<Flower> flowers;
    int flowerTimer = 0;
    int maxFlowers = W / 6 < 3 ? 3 : W / 6;

    printf("\033[?25l\033[2J");
    std::string frame; frame.reserve((size_t)W*H*12);

    while (g_running.load()) {
        int nw, nh; term_size(nw, nh);
        if (nw != W || nh != H) {
            W = nw; H = nh; skyRows = H - GROUND_ROWS;
            if (skyRows < 3) break;
            printf("\033[2J");
        }

        std::vector<std::string> cell(W*H, " ");
        std::vector<const char*> col(W*H, RESET);
        std::vector<int> landRow(W, skyRows); // row index rain lands on, per column (ground by default)

        for (int r = 0; r < GROUND_ROWS; ++r) {
            int y = skyRows + r;
            for (int x = 0; x < W; ++x) { cell[y*W+x] = "="; col[y*W+x] = (r==0) ? BROWN : BROWN_DARK; }
        }

        // ---- hut (walls, tapered roof, door, chimney) ----
        bool hasHut = (W >= 24 && skyRows >= 8);
        int chimneyX = -1, smokeSpawnY = -1;
        if (hasHut) {
            int wallWidth  = 9;
            int wallHeight = 4;
            int roofRows   = 4; // tapers to a 1-wide peak
            int hutX       = W - wallWidth - 3;
            int wallTopRow = skyRows - wallHeight;

            auto place = [&](int x, int y, const char* ch, const char* color) {
                if (x < 0 || x >= W || y < 0 || y >= H) return;
                cell[y*W+x] = ch; col[y*W+x] = color;
                if (y < landRow[x]) landRow[x] = y;
            };

            // walls
            for (int y = wallTopRow; y < skyRows; ++y)
                for (int x = hutX; x < hutX + wallWidth; ++x)
                    place(x, y, "#", HUT_WALL);

            // door (bottom two rows, centered)
            int doorX = hutX + wallWidth / 2;
            place(doorX, skyRows - 1, "|", HUT_DOOR);
            place(doorX, skyRows - 2, "|", HUT_DOOR);

            // tapered roof
            for (int r = 1; r <= roofRows; ++r) {
                int y = wallTopRow - r;
                int left = hutX + (r - 1), right = hutX + wallWidth - 1 - (r - 1);
                for (int x = left; x <= right; ++x) place(x, y, "#", HUT_ROOF);
            }

            // chimney, poking out of the roof near the right side
            chimneyX = hutX + wallWidth - 2;
            int chimneyHeight = 3;
            int chimneyBaseRow = wallTopRow - 1;
            int chimneyTopRow  = chimneyBaseRow - chimneyHeight + 1;
            for (int y = chimneyTopRow; y <= chimneyBaseRow; ++y) place(chimneyX, y, "#", CHIMNEY_COL);
            smokeSpawnY = chimneyTopRow - 1;
        }

        // ---- smoke: spawn, drift upward, fade ----
        if (hasHut && ++smokeTimer >= 6) {
            smokeTimer = 0;
            smoke.push_back({(double)chimneyX + rnd(-0.2, 0.2), (double)smokeSpawnY, rnd(-0.08, 0.08),
                              0, (int)rnd(35, 60)});
        }
        for (auto& s : smoke) {
            int x = (int)std::lround(s.x), y = (int)std::lround(s.y);
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            double frac = 1.0 - (double)s.life / s.maxLife;
            const char* c  = (frac < 0.5) ? "#" : (frac < 0.8 ? "#" : ".");
            const char* co = (frac < 0.35) ? SMOKE1 : (frac < 0.7 ? SMOKE2 : SMOKE3);
            cell[y*W+x] = c; col[y*W+x] = co;
        }

        // ---- flowers: stem grows up from the ground, bloom caps a full-grown one ----
        for (auto& f : flowers) {
            if (f.x < 0 || f.x >= W) continue;
            for (int h = 1; h <= f.height; ++h) {
                int y = skyRows - h;
                if (y < 0) break;
                bool isTop = (h == f.height);
                cell[y*W+f.x] = (isTop && f.height == f.maxHeight) ? "@" : "|";
                col[y*W+f.x]  = (isTop && f.height == f.maxHeight) ? FLOWER_BLOOM : FLOWER_STEM;
            }
        }

        // drops + trails
        for (auto& d : drops) {
            int x = (int)d.x; if (x < 0 || x >= W) continue;
            for (int t = 0; t < d.len; ++t) {
                int y = (int)d.y - t; if (y < 0 || y >= landRow[x]) continue;
                cell[y*W+x] = ":"; col[y*W+x] = (t==0) ? BLUE_BRIGHT : (t < d.len/2+1 ? BLUE_MID : BLUE_DIM);
            }
        }

        for (auto& s : splashes) {
            if (s.x < 0 || s.x >= W || s.y < 0 || s.y >= H) continue;
            cell[s.y*W+s.x] = (s.life > 1) ? "\"" : "."; col[s.y*W+s.x] = SPLASH_COL;
        }

        frame.clear(); frame += "\033[H";
        const char* cur = nullptr;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const char* c = col[y*W+x];
                if (c != cur) { frame += c; cur = c; }
                frame += cell[y*W+x];
            }
            if (y != H-1) frame += "\n";
        }
        frame += RESET;
        fwrite(frame.data(), 1, frame.size(), stdout);
        fflush(stdout);

        // ---- update drops (land on hut roof/chimney or ground, whichever is under them) ----
        for (auto& d : drops) {
            d.y += d.speed;
            int x = (int)d.x;
            int stopRow = (x >= 0 && x < W) ? landRow[x] : skyRows;
            if ((int)d.y - d.len >= stopRow) {
                if (x >= 0 && x < W) {
                    splashes.push_back({x, stopRow, 3});
                    for (size_t fi = 0; fi < flowers.size(); ++fi) {
                        if (flowers[fi].x == x) { flowers[fi] = flowers.back(); flowers.pop_back(); break; }
                    }
                }
                d.x = rnd(0, W-1); d.y = rnd(-6, 0); d.speed = rnd(0.5, 1.6); d.len = 2 + (int)rnd(0,4);
            }
        }
        for (size_t i = 0; i < splashes.size();) {
            if (--splashes[i].life <= 0) { splashes[i] = splashes.back(); splashes.pop_back(); } else ++i;
        }

        // ---- update smoke ----
        for (auto& s : smoke) { s.y -= 0.15; s.x += s.vx; s.life++; }
        for (size_t i = 0; i < smoke.size();) {
            if (smoke[i].life >= smoke[i].maxLife || smoke[i].y < -2) { smoke[i] = smoke.back(); smoke.pop_back(); }
            else ++i;
        }

        // ---- flowers: spawn at a free, hut-free column; grow gradually ----
        if (++flowerTimer >= 15) {
            flowerTimer = 0;
            if ((int)flowers.size() < maxFlowers) {
                int fx = (int)rnd(0, W - 1);
                bool free = (landRow[fx] == skyRows);
                for (auto& f : flowers) if (f.x == fx) { free = false; break; }
                if (free) flowers.push_back({fx, 0, 1 + (int)rnd(0, 3), 0});
            }
        }
        for (auto& f : flowers) {
            if (f.height < f.maxHeight && ++f.growTimer >= 5) { f.growTimer = 0; f.height++; }
        }

        usleep(FRAME_US);
    }
    printf("%s\033[?25h\033[2J\033[H", RESET);
    fflush(stdout);
}

// ================= Audio =================
struct RainDSP {
    static const int FS = 44100;
    static const int TAPS = FS / 200;
    std::vector<float> ring; int ringPos = 0; double sum = 0.0;
    std::mt19937 rng{std::random_device{}()};
    std::normal_distribution<double> hissDist{0.0, 0.3};
    std::normal_distribution<double> spikeAmp{1.0, 0.5};
    std::uniform_real_distribution<double> uni{0.0, 1.0};
    double spikeProb = 1000.0 / FS;
    double gain = 6.0, spikeGain = 0.12;
    RainDSP() : ring(TAPS, 0.0f) {}
    float nextSample() {
        double in = hissDist(rng);
        sum += in - ring[ringPos]; ring[ringPos] = (float)in; ringPos = (ringPos + 1) % TAPS;
        double out = (sum / TAPS) * gain;
        if (uni(rng) < spikeProb) out += spikeAmp(rng) * spikeGain;
        if (out > 1.0) out = 1.0; if (out < -1.0) out = -1.0;
        return (float)out;
    }
};

static void data_callback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount) {
    RainDSP* dsp = (RainDSP*)pDevice->pUserData;
    float* out = (float*)pOutput;
    for (ma_uint32 i = 0; i < frameCount; ++i) out[i] = dsp->nextSample();
}

// ================= Main =================
int main() {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    RainDSP dsp;
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = ma_format_f32;
    cfg.playback.channels = 1;
    cfg.sampleRate        = RainDSP::FS;
    cfg.dataCallback      = data_callback;
    cfg.pUserData         = &dsp;

    ma_device device;
    bool audioOk = (ma_device_init(NULL, &cfg, &device) == MA_SUCCESS);
    if (audioOk) audioOk = (ma_device_start(&device) == MA_SUCCESS);
    if (!audioOk) fprintf(stderr, "Audio unavailable, continuing with visuals only.\n");

    visual_loop();

    if (audioOk) { ma_device_stop(&device); ma_device_uninit(&device); }
    return 0;
}