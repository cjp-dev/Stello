//! Time allocation from Kontrol.cpp (calc_time, calc_end_time, stop_nu). C#: `Search/TimeControl.cs`
//! (`TimeBudget`, `TimeControl`).

use super::limits::{SearchLimits, TimeControlMode};

/// C#: `TimeBudget`. Milliseconds since the search started; `i64::MAX` is no limit.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct TimeBudget {
    /// Do not start a new iteration after this many milliseconds.
    pub soft_ms: i64,
    /// Stop the search after this many milliseconds.
    pub hard_ms: i64,
}

impl TimeBudget {
    /// C#: `TimeBudget.Unlimited`.
    pub const UNLIMITED: TimeBudget = TimeBudget { soft_ms: i64::MAX, hard_ms: i64::MAX };

    /// `SearchOptions.SoftTimePercent`: no new iteration after this share of the budget after `offset_ms`.
    /// C#: `SearchEngine.WithSoftPercent`.
    pub fn with_soft_percent(self, percent: Option<i32>, offset_ms: i64) -> TimeBudget {
        match percent {
            Some(percent) if self.hard_ms != i64::MAX => {
                TimeBudget { soft_ms: offset_ms + (self.hard_ms - offset_ms) * percent as i64 / 100, ..self }
            }
            _ => self,
        }
    }
}

// C++: lookahead - 2 with the default level 8; the last moves take almost no time.
const FAST_ENDGAME_EMPTIES: i64 = 6;

/// C#: `TimeControl.ForMidgame`.
pub fn for_midgame(limits: &SearchLimits, empties: i32) -> TimeBudget {
    match limits.mode() {
        // C++: tider[] is about 2/3 of rtider[].
        TimeControlMode::TimePerMove => TimeBudget { soft_ms: limits.time_ms() * 2 / 3, hard_ms: limits.time_ms() },
        TimeControlMode::TimePerGame => for_game(limits.time_ms(), empties),
        _ => TimeBudget::UNLIMITED,
    }
}

/// With a game clock the endgame search may use half of the time that is left (C++: calc_end_time).
/// C#: `TimeControl.ForEndgame`.
pub fn for_endgame(limits: &SearchLimits, midgame: TimeBudget, elapsed_ms: i64) -> TimeBudget {
    if limits.mode() != TimeControlMode::TimePerGame {
        return midgame;
    }

    let mut rest = limits.time_ms() - elapsed_ms;
    if rest < 0 {
        rest = 10;
    }

    let move_time = rest / 2;
    TimeBudget { soft_ms: elapsed_ms + move_time * 10 / 15, hard_ms: elapsed_ms + move_time }
}

/// The solver's budget with `SoftTimePercent` applied to the remaining window for a game clock.
pub fn for_solver(
    limits: &SearchLimits,
    midgame: TimeBudget,
    elapsed_ms: i64,
    soft_percent: Option<i32>,
) -> TimeBudget {
    let offset_ms = if limits.mode() == TimeControlMode::TimePerGame { elapsed_ms } else { 0 };
    for_endgame(limits, midgame, elapsed_ms).with_soft_percent(soft_percent, offset_ms)
}

// C++: calc_time. Early moves get about 1/4 of the average time per move, the last moves about all of it.
fn for_game(mut remaining_ms: i64, empties: i32) -> TimeBudget {
    let slow_empties = (empties as i64 - FAST_ENDGAME_EMPTIES).max(0);
    let moves_left = slow_empties >> 1;
    if remaining_ms < 0 {
        remaining_ms = 10;
    }

    let mut move_time = if moves_left > 0 { remaining_ms / moves_left } else { remaining_ms };
    move_time = (move_time >> 2) + 3 * move_time * (64 - slow_empties) / 256;
    TimeBudget { soft_ms: move_time * 10 / 15, hard_ms: move_time }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn time_per_move_stops_iterating_after_two_thirds() {
        let budget = for_midgame(&SearchLimits::time_per_move_ms(900), 40);
        assert_eq!(budget, TimeBudget { soft_ms: 600, hard_ms: 900 });
        assert_eq!(budget.with_soft_percent(Some(50), 0), TimeBudget { soft_ms: 450, hard_ms: 900 });
        assert_eq!(TimeBudget::UNLIMITED.with_soft_percent(Some(50), 0), TimeBudget::UNLIMITED);
        assert_eq!(for_midgame(&SearchLimits::fixed_depth(5), 40), TimeBudget::UNLIMITED);
    }

    #[test]
    fn game_clock_matches_kontrol() {
        // 60 s left, 40 empty squares: 17 moves left, 3529 ms average, 882 + 3 * 3529 * 30 / 256 = 2122 ms.
        assert_eq!(
            for_midgame(&SearchLimits::time_per_game_ms(60_000), 40),
            TimeBudget { soft_ms: 1414, hard_ms: 2122 }
        );
        // Time used up: 10 ms are shared out.
        assert_eq!(for_midgame(&SearchLimits::time_per_game_ms(-5), 6), TimeBudget { soft_ms: 6, hard_ms: 9 });
        let midgame = TimeBudget { soft_ms: 1, hard_ms: 2 };
        assert_eq!(for_endgame(&SearchLimits::time_per_move_ms(100), midgame, 50), midgame);
        assert_eq!(
            for_endgame(&SearchLimits::time_per_game_ms(10_000), midgame, 1_000),
            TimeBudget { soft_ms: 4000, hard_ms: 5500 }
        );
    }

    #[test]
    fn endgame_soft_limit_includes_time_already_used() {
        assert_eq!(
            for_solver(&SearchLimits::time_per_game_ms(10_000), TimeBudget::UNLIMITED, 1_000, None),
            TimeBudget { soft_ms: 4_000, hard_ms: 5_500 }
        );
    }

    #[test]
    fn endgame_soft_percent_applies_to_the_remaining_budget() {
        let limits = SearchLimits::time_per_game_ms(10_000);
        for (percent, soft_ms) in [(1, 1_045), (50, 3_250), (100, 5_500)] {
            assert_eq!(
                for_solver(&limits, TimeBudget::UNLIMITED, 1_000, Some(percent)),
                TimeBudget { soft_ms, hard_ms: 5_500 }
            );
        }
    }

    #[test]
    fn solver_budget_preserves_other_time_control_modes() {
        let per_move = SearchLimits::time_per_move_ms(900);
        let budget = for_midgame(&per_move, 40);
        assert_eq!(for_solver(&per_move, budget, 300, None), budget);
        assert_eq!(for_solver(&per_move, budget, 300, Some(50)), TimeBudget { soft_ms: 450, hard_ms: 900 });
        assert_eq!(
            for_solver(&SearchLimits::fixed_depth(5), TimeBudget::UNLIMITED, 300, Some(50)),
            TimeBudget::UNLIMITED
        );
        assert_eq!(for_solver(&SearchLimits::SOLVE, TimeBudget::UNLIMITED, 300, Some(50)), TimeBudget::UNLIMITED);
    }
}
