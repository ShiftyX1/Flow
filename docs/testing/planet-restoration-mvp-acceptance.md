# Planet Restoration MVP — acceptance runbook

Единый автоматический контракт использует `TEST_WORLD_SEED = 424242`. Смена seed считается изменением тестового контракта и должна быть синхронно отражена в `game_content.gd` и C++-тесте генератора.

## Автоматические проверки

Агрегирующая headless-точка входа запускает foundation, protocol/save, gameplay, runtime, UI/world домены и в конце полный MVP acceptance:

```bash
bin/godot.macos.editor.arm64 --headless --path bin/TestVoxels/asdasd --script res://scripts/tests/runtime_smoke.gd
```

Только сквозной сценарий на macOS:

```bash
bin/TestVoxels/asdasd/scripts/tests/run_mvp_acceptance_macos.sh
```

Тот же сценарий на Windows из PowerShell:

```powershell
bin\TestVoxels\asdasd\scripts\tests\run_mvp_acceptance_windows.ps1
```

При нестандартном расположении editor-бинарника путь передаётся первым аргументом shell-скрипту или параметром `-EnginePath` PowerShell-скрипту.

Сценарий создаёт новый Test World и через production-команды проверяет Landing Module, 18 слотов и массу, цепь Generator→Cable→Electric Furnace→Fabricator→Storage, оба пути открытия знаний, оборудование, смерть и единственный Recovery Cache, Beacon Repair Core, активацию и генерацию ресурсов маяком, Fast Travel, autosave, Save & Exit, reload и отсутствие восстановления Sector.

## Ручной editor smoke

На обеих платформах использовать seed `424242` и одну и ту же последовательность:

1. Создать Test World, проверить Landing Module и обычное управление WASD/мышью/прыжком.
2. Открыть инвентарь, взаимодействовать с Landing Module и закрыть панели через Escape.
3. Открыть консоль; проверить `sv_world_info`, `sv_player_info`, затем `sv_cheats 1`, `noclip`, `god` и возврат обоих флагов в `0`.
4. Проверить `cl_show_chunks 1`, `cl_show_power 1`, `cl_show_interactions 1`, `cl_hide_hud 1`; убедиться, что они не меняют мир.
5. Поставить Generator, Cable, Electric Furnace, Fabricator и Storage; проверить связанную визуализацию питания и interaction feedback.
6. Открыть паузу, выполнить «Сохранить и выйти», загрузить мир и убедиться, что Items, знания, оборудование, машины и маяк сохранены.

## Матрица текущей проверки

| Платформа | Автоматический acceptance | Ручной editor smoke |
|---|---|---|
| macOS arm64 | Пройден локально 2026-09-01 | Требуется ручной прогон |
| Windows x86_64 | Runner готов; требуется Windows editor build/host | Требуется ручной прогон |

MVP нельзя объявлять кроссплатформенно принятым, пока обе ячейки Windows и ручные проверки не подтверждены исполнителем.
