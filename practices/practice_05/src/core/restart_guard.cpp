// Реализация подтверждения перезапуска (include/tetris/core/restart_guard.hpp).
//
// Границы: только политика окна. Сброс партии выполняет вызывающая сторона по
// итогу Confirmed — модуль не знает про Game и не трогает рекорд.
#include "tetris/core/restart_guard.hpp"

#include <algorithm>

namespace tetris {

void RestartGuard::clear() {
    state_ = RestartState{};
    elapsed_ms_ = 0;
}

void RestartGuard::open(RestartSource source) {
    state_.active = true;
    state_.source = source;
    state_.window_ms = kWindowMs;
    state_.remaining_ms = kWindowMs;
    state_.progress = 1.0;
    state_.last_close_reason = RestartCloseReason::None;
    elapsed_ms_ = 0;
}

void RestartGuard::close(RestartCloseReason reason) {
    state_.active = false;
    state_.source = RestartSource::None;
    state_.remaining_ms = 0;
    state_.progress = 0.0;
    state_.last_close_reason = reason;
    elapsed_ms_ = 0;
}

RestartOutcome RestartGuard::step(const FrameFacts& facts) {
    RestartOutcome outcome = RestartOutcome::None;

    // 1. Время. Отрицательная дельта не двигает окно назад (FR-022).
    if (state_.active) {
        elapsed_ms_ += std::max(0, facts.delta_ms);
        if (elapsed_ms_ >= kWindowMs) {
            close(RestartCloseReason::Timeout);
            outcome = RestartOutcome::Expired;
        } else {
            const int remaining = kWindowMs - elapsed_ms_;
            state_.remaining_ms = std::clamp(remaining, 0, kWindowMs);
            state_.progress = static_cast<double>(state_.remaining_ms) / kWindowMs;
        }
    }

    // 2. Game over при открытом окне закрывает его без перезапуска (FR-009).
    if (state_.active && facts.game_over) {
        close(RestartCloseReason::GameOver);
        outcome = RestartOutcome::Cancelled;
    }

    // 3. Проглоченная игровая клавиша отменяет окно (FR-006, решения №2 и №3).
    if (state_.active && facts.action_key_pressed) {
        close(RestartCloseReason::PlayerAction);
        outcome = RestartOutcome::Cancelled;
    }

    // 4. Потеря фокуса отменяет окно без изменения партии (FR-008a).
    if (state_.active && facts.focus_lost) {
        close(RestartCloseReason::FocusLost);
        outcome = RestartOutcome::Cancelled;
    }

    // 5. Нажатие R.
    if (facts.r_pressed) {
        if (facts.game_over) {
            // На экране проигрыша окно не нужно: рестарт одним нажатием.
            if (state_.active) {
                close(RestartCloseReason::GameOver);
            }
            state_.last_close_reason = RestartCloseReason::Confirmed;
            outcome = RestartOutcome::Confirmed;
        } else if (state_.active) {
            if (elapsed_ms_ >= kMinIntervalMs) {
                close(RestartCloseReason::Confirmed);
                outcome = RestartOutcome::Confirmed;
            } else {
                // Антидребезг: окно не продлевается и не закрывается.
                outcome = RestartOutcome::Ignored;
            }
        } else {
            open(facts.pause ? RestartSource::Pause : RestartSource::Game);
            outcome = RestartOutcome::Opened;
        }
    }

    return outcome;
}

}  // namespace tetris
