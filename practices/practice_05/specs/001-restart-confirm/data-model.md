# Phase 1 — Data Model: подтверждение перезапуска по R

Все сущности живут в модуле ядра `tetris::RestartGuard`. Ни одна из них не попадает в
`GameSnapshot`, в JSON headless-вывода и в состояние `Game`.

## FrameFacts — факты одного кадра (вход)

| Поле | Тип | Смысл | Кто заполняет |
|---|---|---|---|
| `delta_ms` | `int` | прошедшие мс кадра; отрицательные трактуются как 0 | UI (`SDL_GetTicks`) |
| `r_pressed` | `bool` | R нажато без автоповтора; несколько нажатий в кадре сводятся к одному | UI |
| `action_key_pressed` | `bool` | игровая клавиша нажата и проглочена (не применялась к партии) | UI |
| `pause` | `bool` | партия на паузе | UI |
| `game_over` | `bool` | партия завершена | ядро через `snapshot().game_over` |
| `focus_lost` | `bool` | окно потеряло фокус в этом кадре | UI |

Инварианты: `delta_ms >= 0` после нормализации; `action_key_pressed` несовместим с
применённым действием — если UI проглотил клавишу, он не вызывает `Game::apply`.

## RestartState — состояние для отрисовки (выход)

| Поле | Тип | Диапазон | Смысл |
|---|---|---|---|
| `active` | `bool` | — | окно открыто |
| `remaining_ms` | `int` | 0..2000 | остаток срока; 0 при закрытом окне |
| `window_ms` | `int` | 2000 | длительность окна |
| `progress` | `double` | 0.0..1.0 | `remaining_ms / window_ms` |
| `source` | `RestartSource` | None/Game/Pause | откуда открыто |
| `last_close_reason` | `RestartCloseReason` | см. ниже | почему закрылось последний раз |

## Перечисления

- `RestartOutcome` — итог шага: `None`, `Opened`, `Confirmed` (единственный сигнал к сбросу),
  `Cancelled`, `Expired`, `Ignored`.
- `RestartSource` — `None`, `Game`, `Pause`.
- `RestartCloseReason` — `None`, `Confirmed`, `PlayerAction`, `Timeout`, `GameOver`, `FocusLost`.

## Модель состояний

```
покой ──R (партия идёт)──▶ ожидание(source=Game)
покой ──R (пауза)────────▶ ожидание(source=Pause)
ожидание ──R, ⩾50 мс─────▶ Confirmed ──▶ покой
ожидание ──R, <50 мс──────▶ Ignored (окно не меняется)
ожидание ──⩾2000 мс───────▶ Expired (Timeout) ──▶ покой
ожидание ──игровая клавиша▶ Cancelled (PlayerAction) ──▶ покой
ожидание ──потеря фокуса──▶ Cancelled (FocusLost) ──▶ покой
ожидание ──game over──────▶ Cancelled (GameOver) ──▶ покой
покой ──R (game over)─────▶ Confirmed (окна нет) ──▶ покой
```

## Инварианты

1. `active == true` ⇒ `0 < remaining_ms <= window_ms` и `source != None`.
2. `active == false` ⇒ `remaining_ms == 0` и `progress == 0.0`.
3. `Confirmed` возвращается не более одного раза на окно; после него `active == false`.
4. Открытие окна не меняет партию: `Game` не вызывается вовсе.
5. Сброс партии выполняется вызывающей стороной только по `Confirmed` и только с сидом,
   взятым до сброса.
6. Время монотонно: накопление `elapsed_ms` не уменьшается, отрицательная дельта игнорируется.

## Связь с существующими сущностями

- `Game` / `GameSnapshot` — не меняются; окно не имеет доступа к полю, счёту и очереди.
- `best_recorded` (UI) — не сущность модели, но следствие: после `Confirmed` UI обязан сбросить
  флаг, иначе рекорд новой партии не запишется.
