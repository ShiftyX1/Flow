# Модуль Voxel World — Документация

> **Движок**: Flow (форк Godot 4)  
> **Путь модуля**: `modules/voxel_world/`  
> **Язык**: C++ (API модуля Godot)

---

## Оглавление

1. [Обзор](#1-обзор)
2. [Архитектура](#2-архитектура)
3. [Система блоков](#3-система-блоков)
   - [Перечисление VoxelBlockType](#перечисление-voxelblocktype)
   - [VoxelBlockData (потокобезопасный кэш)](#voxelblockdata-потокобезопасный-кэш)
   - [VoxelBlockRegistry (источник истины)](#voxelblockregistry-источник-истины)
4. [Система биомов](#4-система-биомов)
   - [VoxelBiomeRegistry (API из кода)](#voxelbiomeregistry-api-из-кода)
   - [Параметры биома](#параметры-биома)
   - [Система фичей](#система-фичей)
   - [Смешивание биомов](#смешивание-биомов)
5. [Генерация террейна](#5-генерация-террейна)
   - [VoxelTerrainGenerator](#voxelterraingenerator)
   - [Слои шума](#слои-шума)
6. [Управление чанками](#6-управление-чанками)
   - [VoxelChunk](#voxelchunk)
   - [Загрузка и выгрузка](#загрузка-и-выгрузка)
7. [Система освещения](#7-система-освещения)
   - [Солнечный свет (BFS)](#солнечный-свет-bfs)
   - [Блочный свет (OmniLight3D)](#блочный-свет-omnilight3d)
   - [VoxelLightMap](#voxellightmap)
8. [Мешинг](#8-мешинг)
   - [VoxelMesher](#voxelmesher)
   - [Формат вершин и шейдер](#формат-вершин-и-шейдер)
9. [Нода VoxelWorld (основной API)](#9-нода-voxelworld-основной-api)
   - [Свойства инспектора](#свойства-инспектора)
   - [API для GDScript](#api-для-gdscript)
   - [Цикл дня и ночи](#цикл-дня-и-ночи)
   - [Система тумана](#система-тумана)
10. [Плагин редактора](#10-плагин-редактора)
11. [Практическое руководство](#11-практическое-руководство)
    - [Минимальная настройка сцены](#минимальная-настройка-сцены)
    - [Настройка биомов из GDScript](#настройка-биомов-из-gdscript)
    - [Добавление нового типа блока](#добавление-нового-типа-блока)
    - [Примеры на GDScript](#примеры-на-gdscript)

---

## 1. Обзор

Модуль **voxel_world** добавляет полноценный воксельный движок в Flow:

- Процедурная бесконечная генерация террейна со смешиванием биомов
- **Дата-драйвен система биомов** — движок считает математику (шум, смешивание, пещеры, размещение фичей), а игра конфигурирует параметры через GDScript API
- **Декларативная система фичей** для деревьев (сферическая, конусная, кустовая крона) и разбросанных блоков для каждого биома
- Многопоточная генерация и мешинг чанков через `WorkerThreadPool`
- BFS-распространение солнечного света с попиксельным ambient occlusion
- Динамический блочный свет через `OmniLight3D` от Godot (с тенями)
- Потекстурные и пошейдерные настройки для каждого блока
- Цикл дня и ночи с управлением солнцем, луной и окружением
- Рейкастинг в воксельном пространстве и AABB-коллизия (без `PhysicsServer`)
- Плагин редактора для графического редактирования реестра блоков

### Философия проектирования

Модуль следует чёткому разделению ответственности:

- **Движок** (C++) — вычисление шума, смешивание весов биомов, вырезание пещер, алгоритмы размещения фичей, мешинг, освещение
- **Игра** (GDScript) — определения биомов (форма рельефа, типы блоков, температура/влажность, стили деревьев), конфигурация реестра блоков

Движок поставляется с единственным дефолтным биомом «Луг» (Meadow). Игры добавляют свои биомы (пустыня, лес, горы и т.д.) через API `VoxelBiomeRegistry`.

---

## 2. Архитектура

```
┌──────────────────────────────────────────────────────────┐
│                    VoxelWorld (Node3D)                    │
│  - Владеет чанками, отслеживает камеру, цикл дня/ночи   │
│  - Отправляет задачи генерации в WorkerThreadPool        │
│  - Интегрирует готовые чанки на главном потоке           │
├───────────────┬───────────────┬───────────────────────────┤
│ VoxelTerrain  │ VoxelBlock    │ VoxelBiome                │
│ Generator     │ Registry      │ Registry                  │
│  - Шум        │  - Текстуры   │  - API из кода            │
│  - Смешивание │  - Физика     │  - Параметры биомов       │
│    биомов     │  - Освещение  │  - Климатическая позиция  │
│  - Пещеры     │  - Шейдеры    │  - Фичи (деревья,        │
│  - Фичи      │               │    скаттер)               │
├───────────────┴───────────────┴───────────────────────────┤
│                  Конвейер обработки чанка                  │
│  generate_chunk_data → propagate_sunlight → build_mesh    │
│                                                           │
│  VoxelChunk ←→ VoxelLightMap ←→ VoxelMesher              │
├───────────────────────────────────────────────────────────┤
│          VoxelBlockData (потокобезопасный кэш)            │
│  - Статические массивы, читаемые мешером на рабочих      │
│    потоках                                                │
│  - Заполняется из VoxelBlockRegistry при инициализации    │
└──────────────────────────────────────────────────────────┘
```

**Поток данных для нового чанка:**
1. Камера перемещается → `_update_chunks()` находит отсутствующие позиции чанков
2. `_request_chunk()` отправляет `_chunk_generation_task()` в `WorkerThreadPool`
3. Рабочий поток: `generate_chunk_data()` → `propagate_sunlight()` → `build_chunk_mesh()`
4. Результат помещается в `finished_chunks` (защищён мьютексом)
5. Главный поток: `_integrate_finished_chunks()` применяет меш, сканирует излучающие блоки

---

## 3. Система блоков

### Перечисление VoxelBlockType

Определено в `voxel_block_data.h`. Встроенные ID 0–10:

| ID | Имя      | Твёрдый | Прозрачный | Излучение | Поглощение света |
|----|----------|---------|------------|-----------|------------------|
| 0  | Air      | Нет     | Да         | 0         | 0                |
| 1  | Grass    | Да      | Нет        | 0         | 15               |
| 2  | Dirt     | Да      | Нет        | 0         | 15               |
| 3  | Stone    | Да      | Нет        | 0         | 15               |
| 4  | Sand     | Да      | Нет        | 0         | 15               |
| 5  | Water    | Нет     | Да         | 0         | 2                |
| 6  | Snow     | Да      | Нет        | 0         | 15               |
| 7  | Wood     | Да      | Нет        | 0         | 15               |
| 8  | Leaves   | Да      | Нет        | 0         | 1                |
| 9  | Bedrock  | Да      | Нет        | 0         | 15               |
| 10 | Torch    | Нет     | Да         | 14        | 0                |

`VOXEL_BLOCK_TYPE_MAX = 11`

### VoxelBlockData (потокобезопасный кэш)

Статический класс с изменяемыми массивами. **Не модифицировать напрямую** — заполняется
через `load_from_registry()` при инициализации мира.

```cpp
// Массивы (индексируются по VoxelBlockType):
Color    block_colors[TYPE_MAX];
char*    block_names[TYPE_MAX];       // const, не перезаписывается
bool     block_solid[TYPE_MAX];
bool     block_transparent[TYPE_MAX];
uint8_t  block_emission[TYPE_MAX];
uint8_t  block_light_opacity[TYPE_MAX];
Color    block_light_color[TYPE_MAX];

// Методы запросов (все inline, безопасны из любого потока):
bool    is_solid(VoxelBlockType)
bool    is_transparent(VoxelBlockType)
String  get_block_name(VoxelBlockType)
uint8_t get_block_emission(VoxelBlockType)
uint8_t get_block_light_opacity(VoxelBlockType)
Color   get_block_light_color(VoxelBlockType)
```

### VoxelBlockRegistry (источник истины)

Подкласс `Resource` от Godot, сохраняется как `.tres`. Это **единственный источник истины** для всех определений блоков. Каждый блок — это `BlockEntry`:

| Поле               | Тип              | По умолчанию   | Описание                                  |
|--------------------|------------------|----------------|-------------------------------------------|
| `texture_top`      | `Texture2D`      | null           | Текстура верхней (+Y) грани               |
| `texture_side`     | `Texture2D`      | null           | Текстура боковых граней                   |
| `texture_bottom`   | `Texture2D`      | null           | Текстура нижней (-Y) грани                |
| `shader_material`  | `ShaderMaterial`  | null           | Пользовательский шейдер для блока         |
| `mesh_height`      | `float`          | 1.0            | Визуальная высота (доля 0.0–1.0)          |
| `collision_height` | `float`          | 1.0            | Высота физ. AABB (доля 0.0–1.0)          |
| `color`            | `Color`          | Белый          | Цвет вершины, когда нет текстуры          |
| `solid`            | `bool`           | true           | Блокирует движение, скрывает смежные грани |
| `transparent`      | `bool`           | false          | Пропускает свет                           |
| `light_opacity`    | `int`            | 15             | Поглощение света за шаг (0–15)            |
| `emission`         | `int`            | 0              | Излучаемый блочный свет (0–15)            |
| `light_color`      | `Color`          | Белый          | Цвет OmniLight3D для излучающих блоков    |

**Механизм сохранения**: использует паттерн `_set`/`_get`/`_get_property_list` от Godot (аналогично `MeshLibrary`). Свойства сериализуются как `block/<id>/<field>`.

**Дефолты движка**: при вызове `create_block(id)` для встроенного типа автоматически применяются корректные значения (AIR=нетвёрдый, WATER=прозрачный, TORCH=излучающий и т.д.). При загрузке **старого** `.tres`, в котором нет новых полей, дефолты движка заполняют их. При загрузке **нового** `.tres` сохранённые значения перезаписывают дефолты.

**Ключевые методы** (все привязаны к GDScript через ClassDB):

```gdscript
# CRUD блоков
registry.create_block(id)
registry.remove_block(id)
registry.has_block(id) → bool
registry.get_block_list() → PackedInt32Array
registry.get_block_count() → int

# Текстуры
registry.set_block_texture_top(id, texture)
registry.get_block_texture_top(id) → Texture2D
# ... аналогично для _side, _bottom
registry.get_block_texture_for_face(id, normal) → Texture2D  # авто-выбор грани
registry.block_has_texture(id) → bool

# Шейдер
registry.set_block_shader_material(id, material)
registry.get_block_shader_material(id) → ShaderMaterial
registry.block_has_shader(id) → bool

# Геометрия
registry.set_block_mesh_height(id, 0.875)
registry.set_block_collision_height(id, 0.875)

# Физика и идентификация
registry.set_block_color(id, Color(...))
registry.set_block_solid(id, false)
registry.set_block_transparent(id, true)

# Освещение
registry.set_block_light_opacity(id, 0)
registry.set_block_emission(id, 14)
registry.set_block_light_color(id, Color(1.0, 0.6, 0.2))

# Инициализация всех встроенных типов
registry.setup_defaults()
```

---

## 4. Система биомов

### VoxelBiomeRegistry (API из кода)

`VoxelBiomeRegistry` — класс `RefCounted` (GDCLASS), хранящий определения биомов. Это **не** сериализуемый ресурс — биомы настраиваются исключительно из кода (GDScript или C++).

**Ключевые решения:**
- Движок предоставляет единственный дефолтный биом («Луг») через `setup_defaults()`
- Игры добавляют свои биомы, вызывая `add_biome()` со словарём параметров
- Реестр назначается `VoxelWorld.biome_registry` до инициализации мира
- Если реестр не задан, движок использует встроенный одиночный дефолтный биом

**Жизненный цикл:**
1. Игра создаёт `VoxelBiomeRegistry.new()` в `_init()` скрипта, наследующего `VoxelWorld`
2. Вызывает `setup_defaults()` для добавления встроенного биома «Луг»
3. Вызывает `add_biome()` для каждого дополнительного биома (пустыня, лес, горы и т.д.)
4. Назначает реестр: `biome_registry = reg`
5. При `_ready()` `VoxelWorld` конвертирует записи биомов в `RuntimeBiomeData` и передаёт генератору

**API для GDScript:**

```gdscript
var reg = VoxelBiomeRegistry.new()

# Встроенный дефолт (Луг с деревьями).
reg.setup_defaults()

# Добавить биом.
var id: int = reg.add_biome("desert", {
    "height_base": 0.0,
    "height_scale": 8.0,
    "detail_scale": 1.0,
    "density_3d_weight": 0.3,
    "temperature": 0.8,
    "humidity": -0.6,
    "surface_block": VoxelWorld.VOXEL_BLOCK_SAND,
    "subsurface_block": VoxelWorld.VOXEL_BLOCK_STONE,
    "shore_block": VoxelWorld.VOXEL_BLOCK_SAND,
    "snow_block": VoxelWorld.VOXEL_BLOCK_SNOW,
    "snow_line": 999,
    "features": [
        {
            "type": "tree",
            "trunk_block": VoxelWorld.VOXEL_BLOCK_WOOD,
            "canopy_block": VoxelWorld.VOXEL_BLOCK_LEAVES,
            "density": 700,
            "canopy_shape": "cone",
        }
    ],
})

# Запросы.
reg.get_biome_count()           # → int
reg.get_biome_id("desert")      # → int (или -1)
reg.get_biome_name(0)           # → String
reg.has_biome(0)                # → bool

# Изменение свойств после создания.
reg.set_biome_height_scale(id, 12.0)
reg.set_biome_temperature(id, 0.5)
reg.set_biome_surface_block(id, VoxelWorld.VOXEL_BLOCK_GRASS)

# Управление фичами.
reg.add_biome_feature(id, {"type": "scatter", "block": 10, "density": 200})
reg.get_biome_feature_count(id)   # → int
reg.get_biome_feature(id, 0)      # → Dictionary
reg.clear_biome_features(id)

# Удаление биома (сдвигает ID!).
reg.remove_biome(id)
reg.clear()
```

### Параметры биома

Каждый биом имеет следующие настраиваемые свойства:

| Параметр            | Тип    | Дефолт | Описание                                                |
|---------------------|--------|--------|---------------------------------------------------------|
| `height_base`       | float  | 0.0    | Базовое смещение высоты                                 |
| `height_scale`      | float  | 15.0   | Амплитуда шума континентальности                        |
| `detail_scale`      | float  | 3.0    | Амплитуда шума деталей/эрозии                           |
| `density_3d_weight` | float  | 0.5    | Влияние 3D-шума на террейн (пещеры, навесы)            |
| `temperature`       | float  | 0.0    | Позиция по оси температуры [-1, 1]                      |
| `humidity`          | float  | 0.0    | Позиция по оси влажности [-1, 1]                        |
| `surface_block`     | int    | 1      | ID блока верхнего слоя (напр. трава)                    |
| `subsurface_block`  | int    | 2      | ID блока подповерхностного слоя (напр. земля)           |
| `shore_block`       | int    | 4      | ID блока берега/пляжа (напр. песок)                     |
| `snow_block`        | int    | 6      | ID блока выше линии снега (напр. снег)                  |
| `snow_line`         | int    | 165    | Уровень Y, выше которого поверхность заменяется снегом  |

**Климатическое пространство**: Биомы размещаются в 2D-пространстве (температура, влажность). Генератор сэмплирует шум для получения климатической точки, затем смешивает ближайшие биомы через гауссово взвешивание по расстоянию.

### Система фичей

Каждый биом может иметь ноль или более **фичей** — декларативных описаний того, что размещается на поверхности террейна. Фичи задаются массивом словарей в ключе `"features"` при вызове `add_biome()`, либо добавляются позже через `add_biome_feature()`.

#### Фичи деревьев

Деревья состоят из столбца ствола и кроны (объём листвы) сверху.

| Ключ           | Тип    | Дефолт         | Описание                               |
|----------------|--------|----------------|----------------------------------------|
| `type`         | String | —              | Должно быть `"tree"`                   |
| `trunk_block`  | int    | 7 (Дерево)     | ID блока ствола                        |
| `canopy_block` | int    | 8 (Листва)     | ID блока кроны/листвы                  |
| `density`      | int    | 0              | Хеш-модуль — меньше = плотнее (0 = нет)|
| `min_trunk`    | int    | 4              | Минимальная высота ствола              |
| `max_trunk`    | int    | 6              | Максимальная высота ствола             |
| `canopy_radius`| int    | 2              | Радиус объёма кроны                    |
| `canopy_shape` | String | `"sphere"`     | Одно из: `"sphere"`, `"cone"`, `"bush"`|

**Формы кроны:**

- **`sphere`** — Классическая округлая крона. Сфера с усечёнными углами вокруг верхушки ствола. Подходит для дубов/лиственных деревьев.
- **`cone`** — Форма ели/сосны. Сужающийся конус, широкий снизу и сужающийся к точке. Каждый вертикальный слой уменьшает радиус.
- **`bush`** — Широкая низкая крона. Большой горизонтальный охват с минимальной высотой. Подходит для деревьев в стиле саванны/акации.

**Плотность**: Значение `density` — хеш-модуль. Движок хеширует мировые координаты (x, z) и проверяет `hash % density == 0`. Меньшие значения = больше деревьев. Типичные значения: 15 (густой лес), 400 (редкий луг), 700 (очень редкие — горы). Значение 0 означает, что фича отключена.

#### Фичи скаттера

Скаттер размещает одиночные блоки на поверхности террейна (напр. цветы, грибы, камни).

| Ключ           | Тип    | Дефолт | Описание                                      |
|----------------|--------|--------|-----------------------------------------------|
| `type`         | String | —      | Должно быть `"scatter"`                       |
| `block`        | int    | 0      | ID размещаемого блока                         |
| `density`      | int    | 0      | Хеш-модуль — меньше = плотнее (0 = нет)      |
| `min_y`        | int    | 0      | Минимальный уровень Y для размещения          |
| `max_y`        | int    | 192    | Максимальный уровень Y для размещения         |
| `surface_only` | bool   | true   | Размещать только на поверхности террейна      |

**Соль фичей**: Каждая фича в массиве фичей биома использует уникальную хеш-соль (на основе индекса), чтобы разные фичи не совпадали в одних и тех же позициях.

### Смешивание биомов

Генератор террейна использует шум температуры и влажности для определения климата в каждом столбце. У каждого биома есть центральная точка в пространстве (температура, влажность). Веса биомов вычисляются как:

```
weight[i] = exp(-4.0 * distance_squared(climate_point, biome_center[i]))
```

Веса нормализуются так, чтобы их сумма = 1.0. Параметры террейна (высота, детали, плотность) затем **линейно интерполируются** по всем биомам с этими весами. Это создаёт плавные переходы.

**Несмешиваемые параметры** (блоки, фичи, snow_line) берутся из **доминантного биома** — того, чей вес максимален в данном столбце.

---

## 5. Генерация террейна

### VoxelTerrainGenerator

Процедурная генерация с 3D-density.

**Размер чанка**: 16×192×16 блоков (`CHUNK_SIZE_X/Y/Z`)

### Слои шума

- **Континентальность**: базовая вариация высоты (2D, низкая частота)
- **Эрозия**: детальный шум поверх базы (2D)
- **3D-плотность**: пещеры, навесы, парящие острова (3D)
- **Пещеры-туннели**: `abs(шум) < CAVE_THRESHOLD` → туннель
- **Пещеры-камеры**: `шум > CAVE_CHEESE_THRESHOLD` → большая каверна
- **Шум рек**: создаёт русла рек с берегами
- **Шум озёр**: создаёт впадины с квантованным уровнем воды

**Ключевые константы**:

| Константа                | Значение | Описание                                     |
|--------------------------|----------|----------------------------------------------|
| `BASE_HEIGHT`            | 64.0     | Базовая высота террейна                      |
| `CAVE_THRESHOLD`         | 0.16     | Порог вырезания туннелей                     |
| `CAVE_CHEESE_THRESHOLD`  | 0.26     | Порог вырезания камер                        |
| `RIVER_WIDTH`            | 0.07     | Ширина русла реки                            |
| `LAKE_THRESHOLD`         | 0.35     | Порог образования озёр                       |
| `OCEAN_THRESHOLD`        | -0.2     | Порог глубины океана                         |

**Проходы генерации** (для каждого чанка):

1. **Проход 1**: Вычисление высоты поверхности для каждого столбца (x, z) с использованием смешанных параметров биомов
2. **Проход 2**: Заполнение блоков — бедрок, подповерхность, поверхность, снег, берег. Вырезание пещер и заполнение водой
3. **Проход 3**: Распространение солнечного света (BFS сверху)
4. **Проход 4**: Размещение фичей — итерация по фичам биома для каждого столбца, размещение деревьев и скаттер-блоков

---

## 6. Управление чанками

### VoxelChunk

Хранит блоки и световые данные для одного столбца 16×192×16.

| Свойство       | Тип               | Описание                                    |
|----------------|--------------------|---------------------------------------------|
| `blocks`       | `Vector<uint8_t>`  | Плоский массив, размер = 16×192×16 = 49152  |
| `light_data`   | `Vector<uint8_t>`  | Тот же размер, упаковка по нибблам: ст.=солнце, мл.=блочный |
| `chunk_pos`    | `Vector2i`         | Координаты чанка (не блока)                 |
| `mesh_instance`| `MeshInstance3D*`   | Принадлежащая нода меша в дереве сцены      |

**Индексация блоков**: `index = x + z * SIZE_X + y * SIZE_X * SIZE_Z`

**Кодирование света** (на байт):
- **Старший ниббл** (биты 4–7): уровень солнечного света 0–15
- **Младший ниббл** (биты 0–3): уровень блочного света 0–15

### Загрузка и выгрузка

- Чанки в квадратном радиусе (`chunk_load_radius`) вокруг камеры загружены
- `chunks_per_frame` ограничивает число интегрируемых за кадр чанков (по умолчанию: 4)
- При выгрузке чанков удаляются их излучающие огни и освобождается меш
- Пул рабочих потоков обрабатывает генерацию и ремеш асинхронно

---

## 7. Система освещения

### Солнечный свет (BFS)

Солнечный свет начинается с уровня 15 на вершине чанка (y = SIZE_Y - 1) и распространяется вниз без ослабления через воздух. Горизонтальное распространение снижает уровень на 1 за шаг. Из уровня вычитается `light_opacity` каждого блока.

Вызывается `VoxelLightMap::propagate_sunlight()`. Выполняется на рабочих потоках при генерации чанков.

### Блочный свет (OmniLight3D)

Блоки с `emission > 0` создают ноду Godot `OmniLight3D`:
- `set_shadow(true)` — GPU-тени через воксельный мир
- Энергия = 10.0
- Радиус = `emission * block_size`
- Цвет = `block_light_color` из реестра
- Отслеживается в `HashMap<Vector3i, OmniLight3D*> block_lights`

Управляется методами:
- `_spawn_block_light()` — создаёт ноду света
- `_remove_block_light()` — уничтожает ноду света
- `_scan_chunk_for_lights()` — сканирует новый чанк на излучающие блоки

### VoxelLightMap

Статический служебный класс. Ключевые методы:

```cpp
// Полное распространение света (солнце + блочный BFS)
VoxelLightMap::propagate_all(light_data, blocks, neighbors);

// Только солнечный свет (используется при генерации чанков)
VoxelLightMap::propagate_sunlight(light_data, blocks, neighbors);

// После установки/удаления блока:
VoxelLightMap::remove_and_repropagate_sunlight(light_data, blocks, neighbors);
```

---

## 8. Мешинг

### VoxelMesher

Статическая утилита, строящая поверхности меша из блочных данных чанка.

**Отсечение граней**: грань генерируется только если смежный блок прозрачен (или за границей). Данные соседних чанков передаются через `NeighborBlocks` для бесшовного мешинга.

**Разделение по поверхностям**: блоки группируются по текстуре/шейдеру. Каждый уникальный вариант материала становится отдельной `MeshSurface`. Нетекстурированные блоки объединяются в одну вершинно-раскрашенную поверхность.

**Данные на вершину**:

| Канал    | Формат     | Содержимое                                    |
|----------|------------|-----------------------------------------------|
| `VERTEX` | Vector3    | Позиция в мировом пространстве                |
| `NORMAL` | Vector3    | Нормаль грани                                 |
| `COLOR`  | Color      | Вершинный цвет блока × тонировка текстуры     |
| `UV`     | Vector2    | Текстурные координаты (если текстурирован)    |
| `CUSTOM0`| RGBA8_UNORM| R=солнце, G=блочный_свет, B=AO, A=255        |

**Ambient Occlusion**: вычисляется на вершину методом 3 сэмплов (2 ребра + 1 угловой сосед). Диапазон: 0.34 (полностью затенён) — 1.0 (открыт).

### Формат вершин и шейдер

Встроенный воксельный шейдер:

```glsl
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;
uniform sampler2D texture_albedo : source_color, filter_nearest, repeat_enable;
global uniform float voxel_sun_intensity;
uniform bool use_texture = false;
varying vec4 voxel_light;

void vertex() {
    voxel_light = CUSTOM0;
}

void fragment() {
    vec4 base = use_texture ? texture(texture_albedo, UV) : vec4(1.0);
    ALBEDO = base.rgb * COLOR.rgb;
    float sun = voxel_light.r * voxel_sun_intensity;
    float ao = voxel_light.b;
    float brightness = sun * ao;
    brightness = max(brightness, 0.03);  // Минимальный ambient
    ALBEDO *= brightness;
    if (use_texture) {
        ALPHA = base.a * COLOR.a;
        ALPHA_SCISSOR_THRESHOLD = 0.5;
    }
}
```

`voxel_sun_intensity` обновляется системой цикла дня/ночи как глобальный параметр шейдера.

---

## 9. Нода VoxelWorld (основной API)

### Свойства инспектора

| Категория           | Свойство              | Тип                   | По умолчанию     |
|---------------------|-----------------------|-----------------------|------------------|
| **Мир**             | `seed`                | int                   | -1 (случайный)   |
|                     | `chunk_load_radius`   | int                   | 8                |
|                     | `block_size`          | float                 | 1.0              |
|                     | `sea_level`           | int                   | 52               |
|                     | `chunks_per_frame`    | int                   | 4                |
|                     | `max_pending_chunk_tasks` | int               | 16               |
|                     | `chunk_requests_per_frame` | int              | 8                |
|                     | `remesh_requests_per_frame` | int             | 4                |
|                     | `remeshes_per_frame`  | int                   | 2                |
|                     | `chunk_integration_time_budget_usec` | int      | 2000             |
| **Реестры**         | `block_registry`      | VoxelBlockRegistry    | null             |
|                     | `biome_registry`      | VoxelBiomeRegistry    | null             |
| **Рендеринг**       | `texture_filter`      | TextureFilter         | NEAREST          |
|                     | `alpha_block_flags`   | int (битовое поле)    | 0                |
| **День/ночь**       | `start_time_of_day`   | float (0–24)          | 7.0              |
|                     | `day_length_seconds`  | float                 | 600.0            |
|                     | `auto_advance_time`   | bool                  | false            |
|                     | `time_speed`          | float                 | 1.0              |
| **Туман**           | `fog_enabled`         | bool                  | true             |
|                     | `fog_distance_ratio`  | float                 | 0.85             |
| **Окружение**       | `sun_path`            | NodePath              | ""               |
|                     | `moon_path`           | NodePath              | ""               |
|                     | `environment_path`    | NodePath              | ""               |
| **Отладка**         | `verbose_logging`     | bool                  | true             |
|                     | `show_chunk_borders`  | bool                  | false            |
|                     | `chunk_border_color`  | Color                 | Жёлтый           |

### API для GDScript

```gdscript
# Взаимодействие с блоками
var block_id: int = world.get_block_at(position)
world.set_block_at(position, VoxelWorld.BLOCK_TORCH)
var name: String = world.get_block_name_at(position)

# Конвертация координат
var block_pos: Vector3i = world.world_to_block_pos(world_position)
var world_pos: Vector3 = world.block_to_world_pos(block_pos)

# Запросы биомов
var biome_id: int = world.get_biome_at(position)
var biome_name: String = world.get_biome_name_at(position)

# Рейкастинг (без PhysicsServer)
var result: Dictionary = world.raycast_block(origin, direction, 10.0)
# Возвращает: { "position": Vector3, "normal": Vector3, "block_id": int, "block_pos": Vector3i }
# Возвращает пустой словарь при промахе

# AABB-коллизия (для контроллера персонажа)
var move: Dictionary = world.move_body(aabb, velocity, delta)
# Возвращает: { "position": Vector3, "velocity": Vector3, "on_ground": bool, "in_water": bool }

# День/ночь
world.advance_time(1.0)      # Продвинуть на 1 час
world.is_daytime()            # true если 6:00–18:00
world.is_nighttime()          # true если 18:00–6:00
world.get_day_factor()        # 0.0 (полночь) → 1.0 (полдень)
world.set_time_of_day(12.0)   # Установить полдень
```

### Цикл дня и ночи

Если `auto_advance_time = true`, время идёт в реальном времени на основе `day_length_seconds` и множителя `time_speed`.

Цикл управляет:
- **Солнце** (`DirectionalLight3D` через `sun_path`): вращение, энергия, цветовая температура
- **Луна** (`DirectionalLight3D` через `moon_path`): вращение, энергия
- **Окружение** (`WorldEnvironment` через `environment_path`): ambient свет, небо
- **Шейдер**: глобальный параметр `voxel_sun_intensity` обновляется один раз за кадр

### Система тумана

Туман по расстоянию, масштабированный к дальности отрисовки:
- `fog_distance_ratio = 0.85` означает начало тумана на 85% от `chunk_load_radius * 16 * block_size`
- Управляется через ресурс `Environment`, на который ссылается `environment_path`

---

## 10. Плагин редактора

Модуль регистрирует плагин инспектора для ресурсов `VoxelBlockRegistry`.

**Возможности**:
- Графический диалог редактирования блоков с секциями для каждого блока
- Выбор текстуры для каждого блока (Верх / Бок / Низ)
- Выбор шейдер-материала (пользовательский шейдер для блока)
- Физические параметры: флажок «Твёрдый», флажок «Прозрачный»
- Выбор цвета вершины
- Параметры освещения: Поглощение света (0–15), Излучение (0–15), Цвет света
- Полная интеграция с Undo/Redo через `EditorUndoRedoManager`

Доступ: выберите ресурс `VoxelBlockRegistry` в инспекторе → нажмите «Edit Block Textures & Shaders...»

---

## 11. Практическое руководство

### Минимальная настройка сцены

1. Создайте новую сцену с корневым `Node3D`
2. Добавьте дочернюю ноду `VoxelWorld`
3. Добавьте `Camera3D` как дочернюю ноду `VoxelWorld` (мир отслеживает активную камеру)
4. (Необязательно) Создайте ресурс `VoxelBlockRegistry` (`.tres`) и назначьте его
5. (Необязательно) Прикрепите скрипт, наследующий `VoxelWorld`, который создаёт `VoxelBiomeRegistry` и назначает его в `_init()`
6. (Необязательно) Добавьте ноды `DirectionalLight3D` для солнца/луны и `WorldEnvironment`
7. Запустите сцену — террейн генерируется автоматически

Если `block_registry` не назначен, создаётся по умолчанию с `setup_defaults()`.
Если `biome_registry` не назначен, используется единственный дефолтный биом «Луг».

### Настройка биомов из GDScript

Прикрепите скрипт, наследующий `VoxelWorld`, к ноде VoxelWorld. В `_init()` (который выполняется до `_ready()`) создайте и заполните `VoxelBiomeRegistry`:

```gdscript
extends VoxelWorld

func _init() -> void:
    var reg := VoxelBiomeRegistry.new()
    reg.setup_defaults()  # Добавляет «Луг» (id 0)

    # Пустыня — жаркая, сухая, плоская, без деревьев.
    reg.add_biome("desert", {
        "height_base": 0.0,
        "height_scale": 8.0,
        "detail_scale": 1.0,
        "density_3d_weight": 0.3,
        "temperature": 0.8,
        "humidity": -0.6,
        "surface_block": VoxelWorld.VOXEL_BLOCK_SAND,
        "subsurface_block": VoxelWorld.VOXEL_BLOCK_STONE,
        "shore_block": VoxelWorld.VOXEL_BLOCK_SAND,
        "snow_block": VoxelWorld.VOXEL_BLOCK_SNOW,
        "snow_line": 999,
    })

    # Лес — густые деревья, пышная растительность.
    reg.add_biome("forest", {
        "height_base": 2.0,
        "height_scale": 18.0,
        "detail_scale": 3.0,
        "density_3d_weight": 0.8,
        "temperature": 0.0,
        "humidity": 0.6,
        "surface_block": VoxelWorld.VOXEL_BLOCK_GRASS,
        "subsurface_block": VoxelWorld.VOXEL_BLOCK_DIRT,
        "shore_block": VoxelWorld.VOXEL_BLOCK_SAND,
        "snow_block": VoxelWorld.VOXEL_BLOCK_SNOW,
        "snow_line": 165,
        "features": [
            {
                "type": "tree",
                "trunk_block": VoxelWorld.VOXEL_BLOCK_WOOD,
                "canopy_block": VoxelWorld.VOXEL_BLOCK_LEAVES,
                "density": 15,
                "canopy_shape": "sphere",
            }
        ],
    })

    # Горы — высокие, скалистые, редкие конусные деревья.
    reg.add_biome("mountains", {
        "height_base": 20.0,
        "height_scale": 45.0,
        "detail_scale": 12.0,
        "density_3d_weight": 2.5,
        "temperature": -0.6,
        "humidity": 0.0,
        "surface_block": VoxelWorld.VOXEL_BLOCK_STONE,
        "subsurface_block": VoxelWorld.VOXEL_BLOCK_STONE,
        "shore_block": VoxelWorld.VOXEL_BLOCK_SAND,
        "snow_block": VoxelWorld.VOXEL_BLOCK_SNOW,
        "snow_line": 70,
        "features": [
            {
                "type": "tree",
                "trunk_block": VoxelWorld.VOXEL_BLOCK_WOOD,
                "canopy_block": VoxelWorld.VOXEL_BLOCK_LEAVES,
                "density": 700,
                "canopy_shape": "cone",
            }
        ],
    })

    biome_registry = reg
```

**Важно**: Реестр должен быть назначен в `_init()`, а не `_ready()`, потому что `VoxelWorld` читает `biome_registry` во время `NOTIFICATION_READY`.

### Добавление нового типа блока

1. Добавьте новый элемент в перечисление `VoxelBlockType` в `voxel_block_data.h`:
   ```cpp
   VOXEL_BLOCK_TORCH,
   VOXEL_BLOCK_MY_NEW_BLOCK,  // Добавить перед TYPE_MAX
   VOXEL_BLOCK_TYPE_MAX
   ```

2. Добавьте имя в `voxel_block_data.cpp`:
   ```cpp
   const char *VoxelBlockData::block_names[...] = {
       ...
       "Torch",
       "MyNewBlock",
   };
   ```

3. Обновите все остальные массивы в `voxel_block_data.cpp` значениями по умолчанию.

4. Добавьте дефолты в `VoxelBlockRegistry::create_block()`:
   ```cpp
   case VOXEL_BLOCK_MY_NEW_BLOCK:
       blocks[p_id].color = Color(0.8f, 0.2f, 0.5f);
       blocks[p_id].solid = true;
       // ... и т.д.
       break;
   ```

5. Пересоберите: `scons`

6. Назначьте текстуры в редакторе через диалог Block Registry.

### Примеры на GDScript

**Установка/разрушение блоков с рейкастом:**
```gdscript
func _input(event):
    if event is InputEventMouseButton and event.pressed:
        var camera = get_viewport().get_camera_3d()
        var origin = camera.global_position
        var dir = -camera.global_basis.z
        var hit = voxel_world.raycast_block(origin, dir, 8.0)
        
        if hit.is_empty():
            return
        
        if event.button_index == MOUSE_BUTTON_LEFT:
            # Разрушить блок
            voxel_world.set_block_at(
                voxel_world.block_to_world_pos(hit["block_pos"]), 0)
        
        elif event.button_index == MOUSE_BUTTON_RIGHT:
            # Установить блок на грань
            var place_pos = hit["block_pos"] + Vector3i(hit["normal"])
            voxel_world.set_block_at(
                voxel_world.block_to_world_pos(place_pos), 10)  # Факел
```

**Простой контроллер персонажа:**
```gdscript
func _physics_process(delta):
    var aabb = AABB(global_position - Vector3(0.3, 0, 0.3), Vector3(0.6, 1.8, 0.6))
    var result = voxel_world.move_body(aabb, velocity, delta)
    
    global_position = result["position"]
    velocity = result["velocity"]
    
    if result["on_ground"]:
        if Input.is_action_just_pressed("jump"):
            velocity.y = 8.0
    else:
        velocity.y -= 20.0 * delta
    
    if result["in_water"]:
        velocity.y = max(velocity.y, -2.0)  # Медленное погружение
```
