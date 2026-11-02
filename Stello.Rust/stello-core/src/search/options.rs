//! C#: `Search/SearchOptions.cs`.

use std::sync::Arc;

use crate::eval::EvalWeights;

/// The search's switches, to compare ways of searching in matches and to fit the Multi-ProbCut model.
/// C#: `SearchOptions` (internal there; the trainer's `mpc` and the benchmarks use it).
#[derive(Clone, Debug)]
pub struct SearchOptions {
    /// Another pattern evaluation, or `None` for the built-in [`EvalWeights::built_in`].
    pub pattern: Option<Arc<EvalWeights>>,

    /// No new iteration after this share (1–100) of the hard time limit (`None`: 2/3).
    pub soft_time_percent: Option<i32>,

    /// Multi-ProbCut; off only to measure plain searches (the trainer's mpc command fits its error model).
    pub selective: bool,

    /// Multi-ProbCut's t × 100: a cut needs the shallow search to be t standard deviations outside the window.
    /// `None`: [`super::DEFAULT_PROBCUT_PERCENT`].
    pub probcut_percent: Option<i32>,
}

impl Default for SearchOptions {
    /// C#: `SearchOptions.Default`, the engine as the apps use it.
    fn default() -> SearchOptions {
        SearchOptions { pattern: None, soft_time_percent: None, selective: true, probcut_percent: None }
    }
}
