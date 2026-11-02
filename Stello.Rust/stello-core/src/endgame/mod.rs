//! The endgame solver. C#: `Stello.Engine.V2.Endgame` and the partial class Endgame/SearchEngine.Endgame.cs.
//!
//! - [`search`]: the solver (C# Endgame/SearchEngine.Endgame.cs, and `SolveShallow` from SearchEngine.cs);
//! - [`last`]: the last 4 empty squares (C# `Last4` ... `Last1`);
//! - [`local`]: C# `LocalTable`;
//! - [`stable`]: C# `StableDiscs`;
//! - [`bits`]: C# `EndgameBits`, which the midgame search uses too.

pub mod bits;
mod last;
pub(crate) mod local;
pub mod search;
pub mod stable;
