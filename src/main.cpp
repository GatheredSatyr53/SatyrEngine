// Satyr: a small OpenGL sandbox for raymarching experiments.
//
// The whole scene lives in a fragment shader (shaders/scenes/*.frag). The engine provides a
// window, a fly camera, uniforms, #include support, hot reload, screenshots and a basic
// sphere physics system that collides with the scene's distance field.

#include "engine/Camera.h"
#include "engine/Image.h"
#include "engine/Physics.h"
#include "engine/Renderer.h"
#include "engine/SceneList.h"
#include "engine/SdfProbe.h"
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
#include <random>
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
    double fixedDt = 0.0;           // >0: advance time by this much per frame instead of the clock
    int frames = -1;                // exit after this many frames (-1 = run until closed)
    int spawn = 0;                  // balls dropped at start
    fs::path scene;
    fs::path shaderDir;
    fs::path screenshot;            // when set, saved on the last frame (or when --frames is given)
    bool vsync = true;
    bool physics = true;
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
        "      --dt S           fixed time step per frame (deterministic offline rendering)\n"
        "      --frames N       render N frames and exit (handy with --screenshot)\n"
        "      --screenshot F   save a PNG to F before exiting\n"
        "      --spawn N        drop N physics balls at start\n"
        "      --no-physics     disable the physics system\n"
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
        "  P / T                 pause / reset scene time (physics pauses too)\n"
        "  B / G / X             throw a ball / drop a handful / clear all balls\n"
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
        } else if (a == "--dt") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.fixedDt = std::atof(v);
        } else if (a == "--frames") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.frames = std::atoi(v);
        } else if (a == "--spawn") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.spawn = std::atoi(v);
        } else if (a == "--screenshot") {
            if (!(v = needValue(i, a.c_str()))) return false;
            opt.screenshot = v;
        } else if (a == "--no-vsync") {
            opt.vsync = false;
        } else if (a == "--no-physics") {
            opt.physics = false;
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option: %s\n", a.c_str());
            return false;
        } else {
            opt.scene = a;
        }
    }
    if (opt.width <= 0 || opt.height <= 0 || opt.scale <= 0.0f || opt.fixedDt < 0.0) {
        std::fprintf(stderr, "invalid window size, scale or time step\n");
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
// Returns the point the camera looks at (used as the drop point for balls).
vec3 applySceneView(const Shader& shader, Camera& camera)
{
    camera.reset();
    const SceneView view = parseSceneView(shader.directives());
    if (view.hasPos) camera.position = view.pos;
    const vec3 target = view.hasTarget ? view.target : vec3(0.0f, 0.5f, 0.0f);
    camera.lookAt(target);
    if (view.fov > 0.0f) camera.fov = radians(clamp(view.fov, 10.0f, 170.0f));
    if (view.speed > 0.0f) camera.moveSpeed = view.speed;
    return target;
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

// Everything the scene shaders read each frame (the physics probe gets the same values).
struct FrameUniforms {
    vec2 resolution;
    float time = 0.0f;
    float deltaTime = 0.0f;
    int frame = 0;
    float mouse[4] = {0, 0, 0, 0};
    vec3 camPos;
    mat3 camBasis;
    float fov = 1.0f;
};

void uploadSceneUniforms(Shader& shader, const FrameUniforms& u)
{
    shader.set("uResolution", u.resolution);
    shader.set("uTime", u.time);
    shader.set("uDeltaTime", u.deltaTime);
    shader.set("uFrame", u.frame);
    shader.set("uMouse", u.mouse[0], u.mouse[1], u.mouse[2], u.mouse[3]);
    shader.set("uCamPos", u.camPos);
    shader.set("uCamBasis", u.camBasis);
    shader.set("uCamFov", u.fov);
}

void uploadBodies(Shader& shader, const Physics& physics)
{
    static std::vector<float> packed;
    const std::vector<Body>& bodies = physics.bodies();
    packed.resize(bodies.size() * 4);
    for (size_t i = 0; i < bodies.size(); ++i) {
        packed[i * 4 + 0] = bodies[i].position.x;
        packed[i * 4 + 1] = bodies[i].position.y;
        packed[i * 4 + 2] = bodies[i].position.z;
        packed[i * 4 + 3] = bodies[i].radius;
    }
    shader.set("uBodyCount", static_cast<int>(bodies.size()));
    shader.setVec4Array("uBodies[0]", packed.data(), static_cast<int>(bodies.size()));
    vec3 center;
    float radius = 0.0f;
    if (physics.boundingSphere(center, radius)) shader.set("uBodyBounds", center.x, center.y, center.z, radius);
}

class BallFactory {
public:
    BallFactory() : m_rng(1234u) {}

    // Launches a ball from just in front of the camera along its view direction.
    bool throwFrom(const Camera& camera, Physics& physics)
    {
        Body b;
        b.radius = randomRadius();
        b.position = camera.position + camera.forward() * (0.6f + b.radius);
        b.velocity = camera.forward() * 12.0f;
        b.mass = b.radius * b.radius * b.radius * 40.0f;
        b.restitution = 0.55f;
        return physics.add(b);
    }

    // Drops `count` balls in a loose cloud above `point`.
    int dropAbove(const vec3& point, int count, Physics& physics)
    {
        std::uniform_real_distribution<float> spread(-1.5f, 1.5f);
        std::uniform_real_distribution<float> lift(3.0f, 6.0f);
        int added = 0;
        for (int i = 0; i < count; ++i) {
            Body b;
            b.radius = randomRadius();
            b.position = point + vec3(spread(m_rng), lift(m_rng), spread(m_rng));
            b.velocity = vec3(0.0f, 0.0f, 0.0f);
            b.mass = b.radius * b.radius * b.radius * 40.0f;
            b.restitution = 0.5f;
            if (!physics.add(b)) break;
            ++added;
        }
        return added;
    }

private:
    float randomRadius()
    {
        std::uniform_real_distribution<float> r(0.22f, 0.42f);
        return r(m_rng);
    }

    std::mt19937 m_rng;
};

// First informative line of a shader error (skips the "error in file:" header).
std::string errorSummary(const std::string& err)
{
    std::istringstream in(err);
    std::string line, first;
    while (std::getline(in, line)) {
        const size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (first.empty()) first = line;
        if (line.back() != ':') return line;
    }
    return first;
}

// Physics runs only when the probe compiled and the scene actually draws the bodies.
// Reports the outcome once per (re)load.
bool refreshPhysicsState(const SdfProbe& probe, Shader& shader, const ShaderPreprocessor& pp, const fs::path& scene)
{
    const std::string name = pp.displayName(scene);
    if (!probe.valid()) {
        std::printf("[physics] unavailable for %s: %s\n", name.c_str(), errorSummary(probe.lastError()).c_str());
        return false;
    }
    if (shader.valid() && shader.uniform("uBodyCount") < 0) {
        std::printf("[physics] unavailable for %s: scene does not use common/bodies.glsl\n", name.c_str());
        return false;
    }
    std::printf("[physics] enabled for %s\n", name.c_str());
    return true;
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

    const std::string prelude = "#define SATYR_ENGINE 1\n#define SATYR_MAX_BODIES " + std::to_string(kMaxBodies) + "\n";

    Shader shader(preprocessor);
    shader.setPrelude(prelude);
    shader.load(vertexFile, scenes.current());

    Physics physics;
    BallFactory balls;
    SdfProbe probe(preprocessor, kMaxBodies);
    bool physicsEnabled = opt.physics;   // the system as a whole (--no-physics, probe init)
    bool physicsActive = false;          // usable with the current scene
    if (physicsEnabled && !probe.init()) physicsEnabled = false;
    if (physicsEnabled) {
        probe.load(vertexFile, scenes.current(), prelude);
        physicsActive = refreshPhysicsState(probe, shader, preprocessor, scenes.current());
    }

    Camera camera;
    vec3 sceneTarget = applySceneView(shader, camera);
    if (physicsEnabled && opt.spawn > 0) balls.dropAbove(sceneTarget, opt.spawn, physics);

    printControls();

    float renderScale = opt.scale;
    double sceneTime = opt.startTime;
    int frame = 0;
    bool paused = false;
    bool captureToggled = false;
    bool screenshotRequested = false;
    bool shaderErrorShown = false;

    double lastTime = glfwGetTime();
    const double runStart = lastTime;
    double titleTimer = lastTime;
    int titleFrames = 0;
    double titleTime = 0.0;

    std::vector<SurfaceSample> field;
    std::vector<vec3> bodyPositions;

    while (!window.shouldClose()) {
        window.pollEvents();
        const InputState& in = window.input();

        const double now = glfwGetTime();
        const float dt = opt.fixedDt > 0.0 ? static_cast<float>(opt.fixedDt)
                                           : static_cast<float>(std::min(now - lastTime, 0.1));
        lastTime = now;

        // ---- Input -------------------------------------------------------------------------------
        if (in.pressed(GLFW_KEY_ESCAPE)) {
            if (captureToggled) captureToggled = false;
            else window.requestClose();
        }
        if (in.pressed(GLFW_KEY_TAB)) captureToggled = !captureToggled;
        const bool lookActive = captureToggled || in.mouse(GLFW_MOUSE_BUTTON_RIGHT);
        window.setCursorCaptured(lookActive);

        bool sceneChanged = false;
        if (in.pressed(GLFW_KEY_R)) {
            shader.reload();
            if (physicsEnabled) {
                probe.reload();
                physicsActive = refreshPhysicsState(probe, shader, preprocessor, scenes.current());
            }
            shaderErrorShown = false;
        }
        if (in.pressed(GLFW_KEY_P)) paused = !paused;
        if (in.pressed(GLFW_KEY_T)) { sceneTime = 0.0; frame = 0; }
        if (in.pressed(GLFW_KEY_HOME)) sceneTarget = applySceneView(shader, camera);
        if (in.pressed(GLFW_KEY_F1)) printControls();
        if (in.pressed(GLFW_KEY_F2) || in.pressed(GLFW_KEY_F12)) screenshotRequested = true;
        if (in.pressed(GLFW_KEY_F11)) window.toggleFullscreen();
        if (in.pressed(GLFW_KEY_V)) window.setVsync(!window.vsync());

        if (in.pressed(GLFW_KEY_RIGHT_BRACKET) || in.pressed(GLFW_KEY_PAGE_DOWN) ||
            in.pressed(GLFW_KEY_LEFT_BRACKET) || in.pressed(GLFW_KEY_PAGE_UP)) {
            if (in.pressed(GLFW_KEY_RIGHT_BRACKET) || in.pressed(GLFW_KEY_PAGE_DOWN)) scenes.next();
            else scenes.previous();
            shader.load(vertexFile, scenes.current());
            sceneTarget = applySceneView(shader, camera);
            shaderErrorShown = false;
            sceneChanged = true;
        }
        if (in.pressed(GLFW_KEY_MINUS) || in.pressed(GLFW_KEY_EQUAL)) {
            size_t idx = nearestScaleIndex(renderScale);
            if (in.pressed(GLFW_KEY_MINUS) && idx > 0) --idx;
            if (in.pressed(GLFW_KEY_EQUAL) && idx + 1 < kRenderScales.size()) ++idx;
            renderScale = kRenderScales[idx];
            std::printf("[renderer] render scale %s\n", formatScale(renderScale).c_str());
        }

        if (physicsEnabled) {
            if (sceneChanged) {
                physics.clear();
                probe.load(vertexFile, scenes.current(), prelude);
                physicsActive = refreshPhysicsState(probe, shader, preprocessor, scenes.current());
            }
            if (physicsActive) {
                if (in.pressed(GLFW_KEY_B) && !balls.throwFrom(camera, physics))
                    std::printf("[physics] body limit (%d) reached\n", kMaxBodies);
                if (in.pressed(GLFW_KEY_G)) balls.dropAbove(camera.position + camera.forward() * 4.0f, 8, physics);
                if (in.pressed(GLFW_KEY_X)) physics.clear();
            }
        }

        camera.update(in, dt, lookActive);
        if (!paused) sceneTime += dt;

        if (shader.pollHotReload(now)) {
            shaderErrorShown = false;
            if (physicsEnabled) {
                probe.reload();
                physicsActive = refreshPhysicsState(probe, shader, preprocessor, scenes.current());
            }
        }
        if (!shader.valid() && !shaderErrorShown) {
            std::fprintf(stderr, "[shader] no valid program; fix the error above and save to retry\n");
            shaderErrorShown = true;
        }

        // ---- Frame uniforms ----------------------------------------------------------------------
        renderer.beginFrame(window.framebufferWidth(), window.framebufferHeight(), renderScale);

        FrameUniforms u;
        {
            int winW = 1, winH = 1;
            glfwGetWindowSize(window.handle(), &winW, &winH);
            const float rw = static_cast<float>(renderer.renderWidth());
            const float rh = static_cast<float>(renderer.renderHeight());
            u.resolution = vec2(rw, rh);
            u.time = static_cast<float>(sceneTime);
            u.deltaTime = paused ? 0.0f : dt;
            u.frame = frame;
            u.mouse[0] = static_cast<float>(in.mouseX) * rw / static_cast<float>(std::max(winW, 1));
            u.mouse[1] = (static_cast<float>(winH) - static_cast<float>(in.mouseY)) * rh / static_cast<float>(std::max(winH, 1));
            u.mouse[2] = in.mouse(GLFW_MOUSE_BUTTON_LEFT) ? 1.0f : 0.0f;
            u.mouse[3] = in.mouse(GLFW_MOUSE_BUTTON_RIGHT) ? 1.0f : 0.0f;
            u.camPos = camera.position;
            u.camBasis = camera.basis();
            u.fov = camera.fov;
        }

        // ---- Physics -----------------------------------------------------------------------------
        if (physicsActive) {
            // Samples requested last frame describe the field at the bodies' current positions.
            probe.fetch(field);
            if (!paused) physics.step(dt, field);
            physics.positions(bodyPositions);
            probe.submit(bodyPositions, [&](Shader& s) { uploadSceneUniforms(s, u); });
            // The probe rendered into its own framebuffer; restore this frame's target.
            renderer.beginFrame(window.framebufferWidth(), window.framebufferHeight(), renderScale);
        }

        // ---- Render ------------------------------------------------------------------------------
        if (shader.valid()) {
            shader.bind();
            uploadSceneUniforms(shader, u);
            uploadBodies(shader, physics);
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
            const double fps = titleFrames / std::max(now - titleTimer, 1e-6);
            const double ms = 1000.0 * (now - titleTimer) / std::max(titleFrames, 1);
            char buf[512];
            std::snprintf(buf, sizeof(buf), "Satyr | %s | %dx%d (x%s) | %.0f fps / %.1f ms | t=%.1fs | %d bodies%s%s",
                          preprocessor.displayName(scenes.current()).c_str(),
                          renderer.renderWidth(), renderer.renderHeight(), formatScale(renderScale).c_str(),
                          fps, ms, sceneTime, static_cast<int>(physics.size()),
                          paused ? " [paused]" : "",
                          shader.valid() ? "" : " | SHADER ERROR (see console)");
            window.setTitle(buf);
            titleTimer = now;
            titleFrames = 0;
            titleTime = 0.0;
        }
    }

    if (frame > 0) {
        const double elapsed = glfwGetTime() - runStart;
        std::printf("[stats] %d frames in %.2f s, %.2f ms/frame average\n", frame, elapsed, 1000.0 * elapsed / frame);
    }
    if (physicsEnabled && !physics.empty()) {
        float lowest = 1e9f;
        for (const Body& b : physics.bodies()) lowest = std::min(lowest, b.position.y);
        std::printf("[physics] %d bodies at exit, lowest centre y = %.3f\n", static_cast<int>(physics.size()), static_cast<double>(lowest));
    }
    return 0;
}
