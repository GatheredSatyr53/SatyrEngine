# SatyrEngine

Небольшая песочница для экспериментов с реймарчингом (sphere tracing) на OpenGL 3.3.

Вся сцена — это один фрагментный шейдер в `shaders/scenes/`. Движок берёт на себя окно,
летающую камеру, uniform'ы, `#include` в GLSL, горячую перезагрузку шейдеров и скриншоты,
чтобы можно было сосредоточиться на самих distance-функциях.

## Что умеет

- **Горячая перезагрузка.** Сохранили `.frag` или любой подключённый `.glsl` — картинка обновилась.
  Если шейдер не компилируется, продолжает работать прошлая версия, а в консоль выводится ошибка
  с настоящим именем файла и номером строки.
- **`#include` и `#pragma once` в GLSL.** Пути ищутся относительно подключающего файла, затем относительно `shaders/`.
- **Летающая камера.** WASD + мышь, колесо меняет скорость. Сцена может задать стартовую точку обзора
  строкой `#pragma satyr camera pos=... target=...`.
- **Библиотека SDF** в `shaders/common/`: примитивы, булевы операции и smooth-min, повторение пространства,
  повороты и деформации, нормали, мягкие тени, ambient occlusion, туман, шум, Shadertoy-совместимость.
- **Масштаб рендера** от 0.125x до 3x: тяжёлые фракталы — в низком разрешении, скриншоты — с суперсэмплингом.
- **Скриншоты в PNG** и рендер из командной строки без окна интерактива (`--frames 1 --screenshot out.png`).
- **Одна зависимость** — GLFW, CMake скачивает её сам. Функции OpenGL загружает собственный мини-лоадер
  (`src/engine/gl.h`), GLAD/GLEW не нужны.

## Сборка

Нужны CMake ≥ 3.16, компилятор с C++17 и драйвер с OpenGL 3.3.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/satyr          # Windows: build\bin\satyr.exe
```

На Linux для сборки GLFW нужны dev-пакеты X11:

```sh
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

Для нативного Wayland добавьте `-DSATYR_GLFW_WAYLAND=ON` (нужны `libwayland-dev libxkbcommon-dev wayland-protocols`);
без этого под Wayland приложение работает через XWayland. Чтобы использовать системный GLFW вместо
скачанного — `-DSATYR_FETCH_GLFW=OFF`.

## Запуск

```sh
satyr                                   # первая сцена из shaders/scenes
satyr mandelbulb                        # сцена по имени или по пути к .frag
satyr basic -w 1920 -h 1080 --scale 0.5 # окно 1080p, рендер в половинном разрешении
satyr basic --frames 1 --time 3 --screenshot shot.png   # один кадр в файл и выход
satyr --help
```

Шейдеры берутся из `./shaders`, если такая папка есть в текущем каталоге, иначе из дерева исходников
(путь зашивается при сборке) — так горячая перезагрузка правит файлы прямо в репозитории.

| Клавиша | Действие |
|---|---|
| ПКМ (зажать) / Tab | Обзор мышью (Tab фиксирует захват курсора, Esc снимает) |
| W A S D, Q/E или Ctrl/Space | Движение; Shift — быстрее, Alt — медленнее |
| Колесо мыши | Скорость движения |
| Home | Вернуть камеру в стартовую позицию сцены |
| `[` `]` или PgUp/PgDn | Предыдущая / следующая сцена |
| R | Перезагрузить шейдеры вручную |
| P / T | Пауза / сброс времени сцены |
| `-` / `=` | Уменьшить / увеличить масштаб рендера |
| F2 или F12 | Скриншот в `./screenshots` |
| F11 | Полный экран |
| V | Вертикальная синхронизация |
| F1 | Напечатать список клавиш |
| Esc | Выход |

В заголовке окна показываются текущая сцена, разрешение, FPS, время сцены и признак ошибки шейдера.

## Своя сцена

Скопируйте `shaders/scenes/template.frag` под новым именем — она появится в списке автоматически.
Контракт с движком минимальный:

```glsl
#version 330 core
#pragma satyr camera pos=0,1.5,5 target=0,0.5,0   // стартовая камера (необязательно)

#include "common/camera.glsl"     // cameraRay(), uniform'ы
#include "common/sdf.glsl"        // примитивы
#include "common/ops.glsl"        // операции
#include "common/lighting.glsl"   // rayMarch(), calcNormal(), тени, AO, туман

out vec4 fragColor;

vec2 map(vec3 p)                  // (расстояние, id материала) — единственное, что обязано быть
{
    return vec2(sdSphere(p - vec3(0.0, 1.0, 0.0), 1.0), 1.0);
}

void main()
{
    vec3 rd = cameraRay(gl_FragCoord.xy);
    Hit h = rayMarch(uCamPos, rd);
    vec3 col = h.hit ? vec3(0.5) + 0.5 * calcNormal(uCamPos + rd * h.t) : vec3(0.1);
    fragColor = vec4(toGamma(col), 1.0);
}
```

Параметры марша задаются `#define` до включения библиотеки: `MAX_STEPS`, `MAX_DIST`, `SURF_EPS`,
`STEP_SCALE` (меньше 1 для «нечестных» полей вроде фракталов и twist), `NORMAL_EPS`.

Шейдер с Shadertoy переносится почти без правок:

```glsl
#include "common/shadertoy.glsl"   // iTime, iResolution, iMouse, iFrame, mainImage()
void mainImage(out vec4 fragColor, in vec2 fragCoord) { ... }
```

### Uniform'ы

| Имя | Тип | Описание |
|---|---|---|
| `uResolution` | `vec2` | Размер рендер-таргета в пикселях |
| `uTime` | `float` | Время сцены в секундах (P — пауза, T — сброс) |
| `uDeltaTime` | `float` | Длительность прошлого кадра |
| `uFrame` | `int` | Номер кадра |
| `uMouse` | `vec4` | xy — курсор в пикселях (начало слева внизу), z — зажата ЛКМ, w — ПКМ |
| `uCamPos` | `vec3` | Позиция камеры |
| `uCamBasis` | `mat3` | Столбцы: right, up, forward |
| `uCamFov` | `float` | Вертикальный угол обзора в радианах |

### Библиотека `shaders/common/`

| Файл | Содержимое |
|---|---|
| `uniforms.glsl` | Объявления uniform'ов |
| `camera.glsl` | `cameraRay()`, `lookAtBasis()`, `basisRay()` для скриптовых камер |
| `sdf.glsl` | Сфера, бокс, тор, цилиндр, капсула, конус, октаэдр, призма, эллипсоид, звено цепи… |
| `ops.glsl` | union/subtract/intersect и их smooth-версии, варианты с материалом (`opU`), `opRepeat`, `opSymX`, `rotX/Y/Z`, `opTwist`, `opBend`, `opOnion`, `opRound` |
| `raymarch.glsl` | `rayMarch()`, `calcNormal()`, `softShadow()`, `calcAO()` |
| `lighting.glsl` | `shadeStandard()`, `skyColor()`, `applyFog()`, `checker()`, тонмаппинг, `stepHeatmap()` |
| `noise.glsl` | Хеши, value noise 2D/3D, `fbm()` |
| `shadertoy.glsl` | Слой совместимости с Shadertoy |

Формулы SDF — по статьям Inigo Quilez (iquilezles.org).

## Сцены

- `template.frag` — минимальная заготовка.
- `basic.frag` — витрина примитивов: smooth union, вычитание, материалы, мягкие тени, AO, туман.
  Зажмите ЛКМ, чтобы увидеть тепловую карту числа шагов.
- `repetition.frag` — бесконечный зал колонн: повторение пространства, корректная обработка соседних
  ячеек при разных объектах в ячейках, точечный свет от каждого шара.
- `mandelbulb.frag` — фрактал Мандельбульб с orbit-trap раскраской.

## Структура

```
src/main.cpp                 аргументы, главный цикл, обработка клавиш, uniform'ы
src/engine/gl.h/.cpp         мини-загрузчик OpenGL 3.3 (список функций — X-макрос, легко расширять)
src/engine/Window.*          GLFW: окно, контекст, ввод, полный экран
src/engine/Camera.*          летающая камера → матрица базиса
src/engine/ShaderPreprocessor.*  #include, #pragma once, #line, #pragma satyr, красивые логи ошибок
src/engine/Shader.*          компиляция, кэш uniform'ов, слежение за файлами
src/engine/Renderer.*        полноэкранный треугольник, offscreen-буфер для масштаба рендера
src/engine/Image.*           запись PNG без зависимостей
src/engine/SceneList.*       список .frag в shaders/scenes
shaders/fullscreen.vert      вершинный шейдер (треугольник из gl_VertexID)
shaders/common/*.glsl        библиотека
shaders/scenes/*.frag        сцены
```

## Идеи на потом

- Панель параметров (Dear ImGui) с автоматическими uniform'ами из `#pragma satyr param`.
- Текстуры и кубмапы для окружения.
- Накопление кадров (прогрессивный path tracing) при неподвижной камере.
- Запись последовательности кадров для видео.
