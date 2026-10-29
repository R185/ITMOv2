// Тесты политики подтверждения перезапуска (specs/001-restart-confirm/).
//
// Модуль не знает про Game, поэтому здесь проверяется только окно: открытие,
// подтверждение, антидребезг, границы времени, отмены. Совпадение сброшенной
// партии со свежей проверяется отдельным тестом через Game + to_json.
#include "tetris/core/restart_guard.hpp"

#include "test_framework.hpp"

#include "tetris/core/game.hpp"
#include "tetris/core/serialize.hpp"

#include <string>

using namespace tetris;

// Печать перечислений для CHECK_EQ: без этого тест-фреймворк не соберётся,
// потому что сравнивает значения через ostream.
std::ostream& operator<<(std::ostream& os, RestartOutcome value) {
    switch (value) {
        case RestartOutcome::None:
            return os << "None";
        case RestartOutcome::Opened:
            return os << "Opened";
        case RestartOutcome::Confirmed:
            return os << "Confirmed";
        case RestartOutcome::Cancelled:
            return os << "Cancelled";
        case RestartOutcome::Expired:
            return os << "Expired";
        case RestartOutcome::Ignored:
            return os << "Ignored";
    }
    return os << "RestartOutcome(?)";
}

std::ostream& operator<<(std::ostream& os, RestartSource value) {
    switch (value) {
        case RestartSource::None:
            return os << "None";
        case RestartSource::Game:
            return os << "Game";
        case RestartSource::Pause:
            return os << "Pause";
    }
    return os << "RestartSource(?)";
}

std::ostream& operator<<(std::ostream& os, RestartCloseReason value) {
    switch (value) {
        case RestartCloseReason::None:
            return os << "None";
        case RestartCloseReason::Confirmed:
            return os << "Confirmed";
        case RestartCloseReason::PlayerAction:
            return os << "PlayerAction";
        case RestartCloseReason::Timeout:
            return os << "Timeout";
        case RestartCloseReason::GameOver:
            return os << "GameOver";
        case RestartCloseReason::FocusLost:
            return os << "FocusLost";
    }
    return os << "RestartCloseReason(?)";
}

namespace {

FrameFacts frame(int delta_ms, bool r = false, bool action = false, bool pause = false,
                 bool game_over = false, bool focus_lost = false) {
    FrameFacts facts;
    facts.delta_ms = delta_ms;
    facts.r_pressed = r;
    facts.action_key_pressed = action;
    facts.pause = pause;
    facts.game_over = game_over;
    facts.focus_lost = focus_lost;
    return facts;
}

// Прогон «прошло ms миллисекунд кадрами по 16 мс» без событий.
void wait_ms(RestartGuard& guard, int ms) {
    int left = ms;
    while (left > 0) {
        const int delta = left > 16 ? 16 : left;
        guard.step(frame(delta));
        left -= delta;
    }
}

}  // namespace

TEST(restart, single_press_opens_window) {
    RestartGuard guard;
    CHECK_EQ(guard.step(frame(16, true)), RestartOutcome::Opened);
    CHECK(guard.active());
    CHECK_EQ(guard.state().remaining_ms, RestartGuard::kWindowMs);
    CHECK_EQ(guard.state().source, RestartSource::Game);
}

TEST(restart, second_press_confirms) {
    RestartGuard guard;
    guard.step(frame(16, true));
    CHECK_EQ(guard.step(frame(800, true)), RestartOutcome::Confirmed);
    CHECK_FALSE(guard.active());
    CHECK_EQ(guard.state().last_close_reason, RestartCloseReason::Confirmed);
}

TEST(restart, boundary_one_ms_before_deadline_confirms) {
    RestartGuard guard;
    guard.step(frame(16, true));
    wait_ms(guard, 1980);  // накоплено 1980 мс, остаток 20 мс
    REQUIRE(guard.active());
    CHECK_EQ(guard.state().remaining_ms, 20);
    CHECK_EQ(guard.step(frame(1, true)), RestartOutcome::Confirmed);
}

TEST(restart, deadline_expires_window) {
    RestartGuard guard;
    guard.step(frame(16, true));
    // Кадр открытия окно не двигает: 125 * 16 = 2000 мс накопления.
    for (int i = 0; i < 125; ++i) {
        guard.step(frame(16));
    }
    CHECK_FALSE(guard.active());
    CHECK_EQ(guard.state().last_close_reason, RestartCloseReason::Timeout);
}

TEST(restart, press_after_expiry_opens_new_window) {
    RestartGuard guard;
    guard.step(frame(16, true));
    wait_ms(guard, 2000);
    CHECK_FALSE(guard.active());
    CHECK_EQ(guard.step(frame(16, true)), RestartOutcome::Opened);
    CHECK(guard.active());
    CHECK_EQ(guard.state().remaining_ms, RestartGuard::kWindowMs);
}

TEST(restart, debounce_ignores_press_within_50ms) {
    RestartGuard guard;
    guard.step(frame(16, true));
    CHECK_EQ(guard.step(frame(20, true)), RestartOutcome::Ignored);
    CHECK(guard.active());
    // Окно не продлено и не обнулено: накоплено те же 20 мс.
    CHECK_EQ(guard.state().remaining_ms, 1980);
}

TEST(restart, action_key_cancels_without_restart) {
    RestartGuard guard;
    guard.step(frame(16, true));
    CHECK_EQ(guard.step(frame(16, false, true)), RestartOutcome::Cancelled);
    CHECK_FALSE(guard.active());
    CHECK_EQ(guard.state().last_close_reason, RestartCloseReason::PlayerAction);
}

TEST(restart, focus_lost_cancels) {
    RestartGuard guard;
    guard.step(frame(16, true));
    CHECK_EQ(guard.step(frame(16, false, false, false, false, true)), RestartOutcome::Cancelled);
    CHECK_EQ(guard.state().last_close_reason, RestartCloseReason::FocusLost);
}

TEST(restart, game_over_press_confirms_immediately) {
    RestartGuard guard;
    // Окно не открывается: рестарт одним нажатием.
    CHECK_EQ(guard.step(frame(16, true, false, false, true)), RestartOutcome::Confirmed);
    CHECK_EQ(guard.state().last_close_reason, RestartCloseReason::Confirmed);
}

TEST(restart, game_over_while_window_open_cancels) {
    RestartGuard guard;
    guard.step(frame(16, true));
    CHECK_EQ(guard.step(frame(16, false, false, false, true)), RestartOutcome::Cancelled);
    CHECK_FALSE(guard.active());
    CHECK_EQ(guard.state().last_close_reason, RestartCloseReason::GameOver);
}

TEST(restart, pause_opens_window_with_pause_source) {
    RestartGuard guard;
    CHECK_EQ(guard.step(frame(16, true, false, true)), RestartOutcome::Opened);
    CHECK_EQ(guard.state().source, RestartSource::Pause);
}

TEST(restart, no_repeat_frames_do_not_confirm) {
    RestartGuard guard;
    guard.step(frame(16, true));
    // Удержание R: автоповторы не передаются как нажатия (это делает UI).
    // Кадры без нажатия не подтверждают сброс — окно только истекает.
    for (int i = 0; i < 200; ++i) {
        const RestartOutcome outcome = guard.step(frame(16));
        CHECK(outcome == RestartOutcome::None || outcome == RestartOutcome::Expired);
    }
    CHECK_FALSE(guard.active());
    CHECK_EQ(guard.state().last_close_reason, RestartCloseReason::Timeout);
}

TEST(restart, confirmed_reported_once) {
    RestartGuard guard;
    guard.step(frame(16, true));
    CHECK_EQ(guard.step(frame(600, true)), RestartOutcome::Confirmed);
    CHECK_EQ(guard.step(frame(16)), RestartOutcome::None);
    CHECK_EQ(guard.step(frame(16)), RestartOutcome::None);
}

TEST(restart, negative_and_huge_delta) {
    RestartGuard guard;
    guard.step(frame(16, true));
    // Отрицательная дельта не двигает окно назад: остаток остаётся полным.
    guard.step(frame(-500));
    CHECK(guard.active());
    CHECK_EQ(guard.state().remaining_ms, RestartGuard::kWindowMs);
    // Огромный скачок закрывает окно одним шагом.
    CHECK_EQ(guard.step(frame(100000)), RestartOutcome::Expired);
    CHECK_FALSE(guard.active());
}

TEST(restart, remaining_and_progress_are_clamped) {
    RestartGuard guard;
    guard.step(frame(16, true));
    guard.step(frame(1500));
    const RestartState& state = guard.state();
    CHECK(state.remaining_ms >= 0);
    CHECK(state.remaining_ms <= RestartGuard::kWindowMs);
    CHECK(state.progress > 0.0);
    CHECK(state.progress <= 1.0);
}

TEST(restart, reset_with_same_seed_matches_fresh_game) {
    // SC-003: партия после подтверждения равна свежей партии с тем же сидом.
    Game game(7);
    game.apply(Action::Left);
    game.apply(Action::HardDrop);
    game.apply(Action::Hold);
    game.advance_ticks(30);
    const std::string before_restart = to_json(game.snapshot(), true);
    const std::uint64_t seed = game.snapshot().seed;

    game.reset(seed);
    Game fresh(seed);
    CHECK(to_json(game.snapshot(), true) == to_json(fresh.snapshot(), true));
    CHECK(before_restart != to_json(game.snapshot(), true));
    CHECK_EQ(game.snapshot().score, 0);
    CHECK_EQ(game.snapshot().lines, 0);
    CHECK_EQ(game.snapshot().level, 1);
    CHECK_EQ(game.snapshot().tick, 0u);
    CHECK_FALSE(game.snapshot().game_over);
}
