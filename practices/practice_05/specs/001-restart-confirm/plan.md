# Implementation Plan: подтверждение перезапуска по клавише R

**Branch**: `practice_05` | **Date**: 2026-10-29 | **Spec**: `specs/001-restart-confirm/spec-ui.md`,
`specs/001-restart-confirm/spec-logic.md` (две спеки двух команд, согласованы в `contract.md`)

**Input**: спеки фичи из `specs/001-restart-confirm/`. Команда 1 отвечала за UI (оверлей,
состояния экрана, тексты), команда 2 — за логику (состояние окна, срок, отмены).

## Summary

Сейчас одиночное нажатие R в SDL-окне мгновенно стирает идущую партию
(`src/ui/sdl_app.cpp`, `case SDLK_r`). Требуется подтверждение: первое R открывает окно на
2000 мс, повторное R внутри окна перезапускает партию с тем же seed, истечение срока, игровая
клавиша, потеря фокуса или game over отменяют окно без потери состояния; на экране проигрыша
рестарт остаётся мгновенным.

Технический подход: политика окна выносится в новый модуль ядра `tetris::RestartGuard`
(детерминированный, без SDL и часов, время — дельта кадра), SDL-слой превращается в сборщик
фактов кадра и потребитель состояния для оверлея. `Game`, `GameSnapshot`, headless JSON и
MCP не меняются.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)

**Primary Dependencies**: SDL2 (только `src/ui/`), стандартная библиотека; новых зависимостей нет

**Storage**: файл рекорда `~/.local/share/tetris/highscore` (существующий, не меняется)

**Testing**: ctest → `tetris_tests`, самописный assert-хедер `tests/test_framework.hpp`;
новый файл тестов подхватывается через `GLOB_RECURSE ... CONFIGURE_DEPENDS`

**Target Platform**: Linux (SDL2-окно и headless), Wayland/X11 через XWayland

**Project Type**: desktop-app с отдельным headless-ядром (single project)

**Performance Goals**: 60 fps; окно подтверждения не добавляет работы на кадр сверх одного
вызова `step()` и одной текстовой отрисовки

**Constraints**: ядро без SDL и без системных часов; размер окна 2000 мс; антидребезг 50 мс;
сборка без новых предупреждений `-Wall -Wextra -Wpedantic`

**Scale/Scope**: 2 новых файла модуля (~150 строк), правки в `src/ui/sdl_app.cpp`
(обработчик событий, игровой цикл, отрисовка оверлея), 1 файл тестов (16 тестов)

## Constitution Check

*GATE: до фазы 0 и повторно после фазы 1.*

| Принцип | Проверка | Статус |
|---|---|---|
| I. Ядро без UI | модуль не включает SDL, не печатает, не читает часы; время — параметр | PASS |
| II. Детерминизм | время приходит снаружи, последовательность итогов задаётся фактами кадра | PASS |
| III. Заголовки — интерфейс | добавлен только новый заголовок, существующие не тронуты | PASS |
| IV. Тест раньше кода | тесты политики написаны и запускаются без дисплея; UI-обвязка проверяется сборкой и запуском | PASS |
| V. Одно изменение — один смысл | фича не меняет JSON, сценарии и MCP | PASS |

Повторная проверка после фазы 1: нарушений нет, раздел Complexity Tracking не заполняется
(в шаблоне он остаётся с пояснением, что пуст).

## Project Structure

### Documentation (this feature)

```text
specs/001-restart-confirm/
├── spec-ui.md            # спека команды 1 (UI)
├── spec-logic.md         # спека команды 2 (логика)
├── spec-*.before.md      # версии спек до согласования контракта
├── contract.md           # общий контракт: владельцы, факты кадра, шесть решений
├── research.md           # Phase 0: факты кода (R1–R8)
├── data-model.md         # Phase 1: FrameFacts, RestartState, модель состояний
├── contracts/
│   └── restart-guard.md  # API модуля и обязанности UI
├── plan.md               # этот файл
├── tasks.md              # Phase 2
├── checklists/
│   └── requirements.md   # проверка качества требований
└── analysis.md           # кросс-артефактный анализ спек/плана/задач
```

### Source Code (repository root)

```text
include/tetris/core/restart_guard.hpp   # новый интерфейс (замороженный)
include/tetris/core/{game,board,pieces,scoring,serialize}.hpp   # без изменений
src/core/restart_guard.cpp              # новая политика окна
src/core/{game,board,pieces,scoring,serialize,events}.cpp       # без изменений
src/ui/sdl_app.cpp                      # сбор фактов кадра, оверлей, приоритет оверлеев
tests/test_restart_guard.cpp            # 16 тестов политики + сверка сброса с Game(seed)
```

**Structure Decision**: single project — ядро в `src/core` + `include/tetris/core`, UI в
`src/ui`, тесты в `tests/`. Новый модуль положен в ядро, а не в UI, потому что сборка
`tetris_tests` не линкуется с SDL: политика вне ядра была бы непроверяема (research R7).

## Complexity Tracking

> Нарушений конституции нет — таблица не заполняется.

Отдельно отмечено, но нарушением не считается: модуль вызывается из UI один раз за кадр
(вместо прямой реакции на событие). Это цена детерминизма и тестируемости: время становится
параметром, а не побочным эффектом обработчика событий.
