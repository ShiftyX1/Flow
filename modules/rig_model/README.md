# rig_model

Data-driven модели и анимации персонажей (Bedrock-подобные форматы).

| Файл | Ресурс | Содержимое |
|---|---|---|
| `*.geo.json` | `RigModelData` | кости, кубы (box UV / per-face UV), локаторы |
| `*.anim.json` | `RigAnimationData` | клипы с ключами и выражениями |
| `*.controller.json` | `RigControllerData` | стейт-машины с переходами и blend |

`ModelRig` (Node3D) собирает риг из `RigModelData`, проигрывает клипы и контроллеры.
Игровой код управляет риггом через запросы: `set_query("is_moving", 1.0)`.
В выражениях они доступны как `query.is_moving` (или `q.is_moving`).

## Конвенции

- Углы в JSON — градусы; X и Y поворота кости инвертируются при переходе в Godot (как в Blockbench).
- Позиции в анимациях: X инвертируется, масштаб `unit_scale` (по умолчанию 1/16).
- Модель смотрит в `-Z`: north = -Z, east = +X.
- Анимации аддитивные: поворот и позиция суммируются с весом, scale — `1 + Σw(s-1)`.
- Клип без `loop` держит последнюю позу (отличие от Bedrock).

## Выражения (Molang-lite)

Арифметика, сравнения, `&&`, `||`, `!`, тернарный оператор, скрипты через `;`
с присваиванием `variable.x = ...;` и `return`. `math.*` работает в градусах.
Запросы контроллеров: `query.state_time`, `query.all_animations_finished`,
`query.any_animation_finished`, `query.anim_phase.<клип>`.

## Нестандартные расширения geo

Уровень кости: `texture`, `texture_size` (несколько текстур на модель).
Уровень description: `texture`, `unit_scale`.

## Тесты

C++: `tests/test_rig_model.h` (запускаются в сборке `tests=yes`).
Интеграционные: `asdasd/scripts/tests/rig_model_smoke.gd` в игровом проекте.
