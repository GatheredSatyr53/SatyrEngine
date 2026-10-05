// Satyr: a small OpenGL sandbox for raymarching experiments.
//
// The whole scene lives in a fragment shader (shaders/scenes/*.frag). The engine provides a
// window, a fly camera, uniforms, #include support, hot reload and screenshots.

#include "engine/Camera.h"
#include "engine/Image.h"
#include "engine/Renderer.h"
#include "engine/SceneList.h"
#include "engine/Shader.h"
#include "engine/ShaderPreprocessor.h"
#include "engine/Window.h"
#include "engine/gl.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace satyr;

namespace {

struct Options {
    int width = 1280;
    int height = 720;
    float scale = 1.0f;
    double startTime = 0.0;
    int frames = -1;                // exit after this many frames (-1 = run until closed)
    fs::path scene;
    fs::path shaderDir;
    fs::path screenshot;            // when set, saved on the last frame (or when --frames is given)
    bool vsync = true;
    bool help = false;
};

constexpr std::array<float, 8> kRenderScales = {0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f};

void printUsage(const char* exe)
{
    std::printf(
        "usage: %s [scene.frag] [options]\n"
        "  -w, --width N        window width (default 1280)\n"
        "  -h, --height N       window height (default 720)\n"
        "  -s, --scale F        render resolution scale (default 1.0)\n"
        "      --shaders DIR    shader root directory (default: ./shaders or the source tree)\n"
        "      --time T         initial scene time in seconds\n"
        "      --frames N       render N frames and exit (handy with --screenshot)\n"
        "      --screenshot F   save a PNG to F before exiting\n"
        "      --no-vsync       disable vertical sync\n"
        "      --help           show this help\n",
        exe);
}

void printControls()
{
    std::printf(
        "\n"
        "controls:\n"
        "  RMB drag / Tab        mouse look (Tab toggles capture, Esc releases it)\n"
        "  W A S D, Q/E or Ctrl/Space   move (Shift = fast, Alt = slow)\n"
        "  mouse wheel           change movement speed\n"
        "  Home                  reset camera\n"
        "  [ ]  or PgUp/PgDn     previous / next scene\n"
        "  R                     reload shaders (they also reload automatically on save)\n"
        "  P / T                 pause / reset scene time\n"
        "  - / =                 lower / raise render resolution scale\n"
        "  F2 or F12             screenshot (saved into ./screenshots)\n"
        "  F11                   toggle fullscreen\n"
        "  V                     toggle vsync\n"
        "  F1                    print this list\n"
        "  Esc                   quit\n\n");
}

bool parseArgs(int argc, char** argv, Options& opt)
{
    auto needValue = [&](int& i, const char* flag) -> const char* {
        if (i + 1 >= argc) {
            std::fprintf(stderr, "missing value for %s\n", flag);
            return nullptr;
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const char* v = nullptr;
        if (a == "--help") {
            opt.help = true;
        } else if (a == "-w" || a == "--width") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.width = std::atoi(v);
        } else if (a == "-h" || a == "--height") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.height = std::atoi(v);
        } else if (a == "-s" || a == "--scale") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.scale = static_cast<float>(std::atof(v));
        } else if (a == "--shaders") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.shaderDir = v;
        } else if (a == "--time") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.startTime = std::atof(v);
        } else if (a == "--frames") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.frames = std::atoi(v);
        } else if (a == "--screenshot") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.screenshot = v;
        } else if (a == "--no-vsync") {
            opt.vsync = false;
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option: %s\n", a.c_str());
            return false;
        } else {
            opt.scene = a;
        }
    }
    if (opt.width <= 0 || opt.height <= 0 || opt.scale <= 0.0f) {
        std::fprintf(stderr, "invalid window size or scale\n");
        return false;
    }
    return true;
}

fs::path findShaderDir(const Options& opt)
{
    if (!opt.shaderDir.empty()) return fs::absolute(opt.shaderDir);
    std::error_code ec;
    if (fs::exists("shaders/fullscreen.vert", ec)) return fs::absolute("shaders");
#ifdef SATYR_SHADER_DIR
    return fs::path(SATYR_SHADER_DIR);
#else
    return fs::absolute("shaders");
#endif
}

// Accepts "scenes/basic.frag", "basic.frag" or just "basic".
fs::path resolveScene(const fs::path& arg, const fs::path& shaderDir)
{
    std::error_code ec;
    const fs::path candidates[] = {
        arg,
        shaderDir / "scenes" / arg,
        shaderDir / "scenes" / (arg.string() + ".frag"),
        shaderDir / arg,
    };
    for (const fs::path& c : candidates)
        if (fs::is_regular_file(c, ec)) return fs::absolute(c).lexically_normal();
    return {};
}

std::string timestamp()
{
    const std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));
    return buf;
}

bool saveScreenshot(const Renderer& renderer, fs::path file)
{
    std::vector<std::uint8_t> rgb;
    int w = 0, h = 0;
    if (!renderer.readWindowPixels(rgb, w, h)) return false;

    if (file.empty()) {
        std::error_code ec;
        fs::create_directories("screenshots", ec);
        file = fs::path("screenshots") / ("satyr_" + timestamp() + ".png");
    } else if (file.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
    }

    const bool ok = writePNG(file, w, h, rgb.data(), true);
    if (ok) std::printf("[screenshot] saved %s (%dx%d)\n", file.string().c_str(), w, h);
    return ok;
}

// Scenes can declare their starting viewpoint:
//   #pragma satyr camera pos=0,1.5,5 target=0,0.5,0 fov=60 speed=3
struct SceneView {
    bool hasPos = false, hasTarget = false;
    vec3 pos, target;
    float fov = 0.0f;
    float speed = 0.0f;
};

bool parseNumbers(const std::string& text, float* out, size_t count)
{
    std::stringstream ss(text);
    for (size_t i = 0; i < count; ++i) {
        std::string tok;
        if (!std::getline(ss, tok, ',') || tok.empty()) return false;
        char* end = nullptr;
        out[i] = std::strtof(tok.c_str(), &end);
        if (end == tok.c_str()) return false;
    }
    return true;
}

SceneView parseSceneView(const std::vector<std::string>& directives)
{
    SceneView view;
    for (const std::string& d : directives) {
        std::istringstream in(d);
        std::string kind;
        in >> kind;
        if (kind != "camera") continue;
        std::string kv;
        while (in >> kv) {
            const size_t eq = kv.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = kv.substr(0, eq);
            const std::string val = kv.substr(eq + 1);
            float v[3] = {0, 0, 0};
            if (key == "pos" && parseNumbers(val, v, 3)) {
                view.pos = {v[0], v[1], v[2]};
                view.hasPos = true;
            } else if (key == "target" && parseNumbers(val, v, 3)) {
                view.target = {v[0], v[1], v[2]};
                view.hasTarget = true;
            } else if (key == "fov" && parseNumbers(val, v, 1)) {
                view.fov = v[0];
            } else if (key == "speed" && parseNumbers(val, v, 1)) {
                view.speed = v[0];
            } else {
                std::fprintf(stderr, "[scene] ignoring camera option '%s'\n", kv.c_str());
            }
        }
    }
    return view;
}

// Resets the camera to the engine default, then applies the scene's "#pragma satyr camera".
void applySceneView(const Shader& shader, Camera& camera)
{
    camera.reset();
    const SceneView view = parseSceneView(shader.directives());
    if (view.hasPos) camera.position = view.pos;
    camera.lookAt(view.hasTarget ? view.target : vec3(0.0f, 0.5f, 0.0f));
    if (view.fov > 0.0f) camera.fov = radians(clamp(view.fov, 10.0f, 170.0f));
    if (view.speed > 0.0f) camera.moveSpeed = view.speed;
}

size_t nearestScaleIndex(float scale)
{
    size_t best = 0;
    for (size_t i = 1; i < kRenderScales.size(); ++i)
        if (std::fabs(kRenderScales[i] - scale) < std::fabs(kRenderScales[best] - scale)) best = i;
    return best;
}

std::string formatScale(float s)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(s));
    return buf;
}

} // namespace

int main(int argc, char** argv)
{
    // Log output is tiny; keep it unbuffered so it shows up immediately in pipes and IDEs too.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        printUsage(argv[0]);
        return 1;
    }
    if (opt.help) {
        printUsage(argv[0]);
        printControls();
        return 0;
    }

    const fs::path shaderDir = findShaderDir(opt);
    const fs::path vertexFile = shaderDir / "fullscreen.vert";
    {
        std::error_code ec;
        if (!fs::is_regular_file(vertexFile, ec)) {
            std::fprintf(stderr, "shader directory not found (looked for %s); use --shaders DIR\n",
                         vertexFile.string().c_str());
            return 1;
        }
    }

    SceneList scenes(shaderDir / "scenes");
    if (!opt.scene.empty()) {
        const fs::path scene = resolveScene(opt.scene, shaderDir);
        if (scene.empty()) {
            std::fprintf(stderr, "scene not found: %s\n", opt.scene.string().c_str());
            return 1;
        }
        scenes.select(scene);
    }
    if (scenes.empty()) {
        std::fprintf(stderr, "no scenes found in %s\n", (shaderDir / "scenes").string().c_str());
        return 1;
    }

    Window window;
    if (!window.create(opt.width, opt.height, "Satyr")) return 1;
    window.setVsync(opt.vsync);

    Renderer renderer;
    if (!renderer.init()) return 1;

    ShaderPreprocessor preprocessor(shaderDir);
    std::printf("[shader] root: %s\n", shaderDir.string().c_str());

    Shader shader(preprocessor);
    shader.load(vertexFile, scenes.current());

    Camera camera;
    applySceneView(shader, camera);

    printControls();

    float renderScale = opt.scale;
    double sceneTime = opt.startTime;
    int frame = 0;
    bool paused = false;
    bool captureToggled = false;
    bool screenshotRequested = false;
    bool shaderErrorShown = false;

    double lastTime = glfwGetTime();
    double titleTimer = lastTime;
    int titleFrames = 0;
    double titleTime = 0.0;

    while (!window.shouldClose()) {
        window.pollEvents();
        const InputState& in = window.input();

        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::min(now - lastTime, 0.1));
        lastTime = now;

        // ---- Input -------------------------------------------------------------------------------
        if (in.pressed(GLFW_KEY_ESCAPE)) {
            if (captureToggled) captureToggled = false;
            else window.requestClose();
        }
        if (in.pressed(GLFW_KEY_TAB)) captureToggled = !captureToggled;
        const bool lookActive = captureToggled || in.mouse(GLFW_MOUSE_BUTTON_RIGHT);
        window.setCursorCaptured(lookActive);

        if (in.pressed(GLFW_KEY_R)) shader.reload();
        if (in.pressed(GLFW_KEY_P)) paused = !paused;
        if (in.pressed(GLFW_KEY_T)) { sceneTime = 0.0; frame = 0; }
        if (in.pressed(GLFW_KEY_HOME)) applySceneView(shader, camera);
        if (in.pressed(GLFW_KEY_F1)) printControls();
        if (in.pressed(GLFW_KEY_F2) || in.pressed(GLFW_KEY_F12)) screenshotRequested = true;
        if (in.pressed(GLFW_KEY_F11)) window.toggleFullscreen();
        if (in.pressed(GLFW_KEY_V)) window.setVsync(!window.vsync());

        if (in.pressed(GLFW_KEY_RIGHT_BRACKET) || in.pressed(GLFW_KEY_PAGE_DOWN) ||
            in.pressed(GLFW_KEY_LEFT_BRACKET) || in.pressed(GLFW_KEY_PAGE_UP)) {
            if (in.pressed(GLFW_KEY_RIGHT_BRACKET) || in.pressed(GLFW_KEY_PAGE_DOWN)) scenes.next();
            else scenes.previous();
            shader.load(vertexFile, scenes.current());
            applySceneView(shader, camera);
            shaderErrorShown = false;
        }
        if (in.pressed(GLFW_KEY_MINUS) || in.pressed(GLFW_KEY_EQUAL)) {
            size_t idx = nearestScaleIndex(renderScale);
            if (in.pressed(GLFW_KEY_MINUS) && idx > 0) --idx;
            if (in.pressed(GLFW_KEY_EQUAL) && idx + 1 < kRenderScales.size()) ++idx;
            renderScale = kRenderScales[idx];
            std::printf("[renderer] render scale %s\n", formatScale(renderScale).c_str());
        }

        camera.update(in, dt, lookActive);
        if (!paused) sceneTime += dt;

        if (shader.pollHotReload(now)) shaderErrorShown = false;
        if (!shader.valid() && !shaderErrorShown) {
            std::fprintf(stderr, "[shader] no valid program; fix the error above and save to retry\n");
            shaderErrorShown = true;
        }

        // ---- Render ------------------------------------------------------------------------------
        renderer.beginFrame(window.framebufferWidth(), window.framebufferHeight(), renderScale);
        if (shader.valid()) {
            int winW = 1, winH = 1;
            glfwGetWindowSize(window.handle(), &winW, &winH);
            const float rw = static_cast<float>(renderer.renderWidth());
            const float rh = static_cast<float>(renderer.renderHeight());
            const float mx = static_cast<float>(in.mouseX) * rw / static_cast<float>(std::max(winW, 1));
            const float my = (static_cast<float>(winH) - static_cast<float>(in.mouseY)) * rh / static_cast<float>(std::max(winH, 1));

            shader.bind();
            shader.set("uResolution", vec2(rw, rh));
            shader.set("uTime", static_cast<float>(sceneTime));
            shader.set("uDeltaTime", paused ? 0.0f : dt);
            shader.set("uFrame", frame);
            shader.set("uMouse", mx, my,
                       in.mouse(GLFW_MOUSE_BUTTON_LEFT) ? 1.0f : 0.0f,
                       in.mouse(GLFW_MOUSE_BUTTON_RIGHT) ? 1.0f : 0.0f);
            shader.set("uCamPos", camera.position);
            shader.set("uCamBasis", camera.basis());
            shader.set("uCamFov", camera.fov);
            renderer.drawFullscreen();
        } else {
            glClearColor(0.35f, 0.0f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        renderer.endFrame();

        const bool lastFrame = opt.frames >= 0 && frame + 1 >= opt.frames;
        if (screenshotRequested || (lastFrame && !opt.screenshot.empty())) {
            glFinish();
            saveScreenshot(renderer, screenshotRequested ? fs::path{} : opt.screenshot);
            screenshotRequested = false;
        }

        window.swapBuffers();
        ++frame;
        if (lastFrame) window.requestClose();

        // ---- Title / stats -----------------------------------------------------------------------
        ++titleFrames;
        titleTime += dt;
        if (now - titleTimer >= 0.5) {
            const double fps = titleFrames / std::max(titleTime, 1e-6);
            const double ms = 1000.0 * titleTime / std::max(titleFrames, 1);
            char buf[512];
            std::snprintf(buf, sizeof(buf), "Satyr | %s | %dx%d (x%s) | %.0f fps / %.1f ms | t=%.1fs%s%s",
                          preprocessor.displayName(scenes.current()).c_str(),
                          renderer.renderWidth(), renderer.renderHeight(), formatScale(renderScale).c_str(),
                          fps, ms, sceneTime,
                          paused ? " [paused]" : "",
                          shader.valid() ? "" : " | SHADER ERROR (see console)");
            window.setTitle(buf);
            titleTimer = now;
            titleFrames = 0;
            titleTime = 0.0;
        }
    }

    return 0;
}
