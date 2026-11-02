//! The JSON messages between the page and the engine worker, the shapes of the web app's `EngineProtocol`
//! (`Stello.Net/Stello.Web/Worker/EngineProtocol.cs`): flat objects with PascalCase names, enums as their C# numbers
//! and times in .NET ticks (100 ns).
//!
//! They are read and written here, not in JavaScript, because the boards are 64-bit: `JSON.parse` turns numbers into
//! doubles and would lose the upper squares.

use std::fmt::Write;
use std::time::Duration;

use stello_core::{Board, Player, SearchInfo, SearchLimits, SearchResult, TimeControlMode};

/// The default of `MoveRequest.HashBits` (C#: `SearchLimits.DefaultHashBits`).
const DEFAULT_HASH_BITS: u32 = 19;

/// C#: `MoveRequest`, without `UseOpeningBook`: the page looks up the book itself and sends only searches.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct MoveRequest {
    pub board: Board,
    pub player: Player,
    pub limits: SearchLimits,
    pub hash_bits: u32,
}

/// Reads a `MoveRequest` (C#: `EngineProtocol.WriteRequest`). Unknown names are skipped.
pub fn read_request(json: &str) -> Result<MoveRequest, String> {
    let mut black = None;
    let mut white = None;
    let mut player = None;
    let mut mode = None;
    let mut depth = 0i64;
    let mut time_ticks = 0i64;
    let mut hash_bits = DEFAULT_HASH_BITS as i64;
    let mut threads = 1i64;

    for (name, value) in Reader::new(json).object()? {
        match name {
            "Black" => black = Some(value.unsigned(name)?),
            "White" => white = Some(value.unsigned(name)?),
            "Player" => player = Some(value.integer(name)?),
            "Mode" => mode = Some(value.integer(name)?),
            "Depth" => depth = value.integer(name)?,
            "TimeTicks" => time_ticks = value.integer(name)?,
            "HashBits" => hash_bits = value.integer(name)?,
            "Threads" => threads = value.integer(name)?,
            _ => {}
        }
    }

    let missing = |name: &str| format!("The request has no {name}.");
    let (black, white) = (black.ok_or_else(|| missing("Black"))?, white.ok_or_else(|| missing("White"))?);
    if black & white != 0 {
        return Err("A square is both black and white.".to_string());
    }

    let player = match player.ok_or_else(|| missing("Player"))? {
        0 => Player::Black,
        1 => Player::White,
        other => return Err(format!("Not a player: {other}.")),
    };
    let mode = match mode.ok_or_else(|| missing("Mode"))? {
        0 => TimeControlMode::FixedDepth,
        1 => TimeControlMode::TimePerMove,
        2 => TimeControlMode::TimePerGame,
        3 => TimeControlMode::Solve,
        other => return Err(format!("Not a time control: {other}.")),
    };
    let depth = i32::try_from(depth).map_err(|_| format!("Not a depth: {depth}."))?;
    // C#'s wrapper passes (long)Time.TotalMilliseconds; integer division truncates towards zero the same way.
    let time_ms = time_ticks / 10_000;
    let limits =
        SearchLimits::from_parts(mode, depth, time_ms).ok_or_else(|| "The limits are not valid.".to_string())?;
    // Checked as C#'s SearchLimits.Threads does; this single-threaded build then searches with 1 (the engine ignores
    // the count where threads are not available).
    let limits = u32::try_from(threads)
        .ok()
        .and_then(|threads| limits.with_threads(threads))
        .ok_or_else(|| format!("Threads must be 1-{}, not {threads}.", SearchLimits::MAX_THREADS))?;
    let hash_bits = match u32::try_from(hash_bits) {
        Ok(bits @ 10..=26) => bits,
        _ => return Err(format!("Hash bits must be 10-26, not {hash_bits}.")),
    };

    Ok(MoveRequest { board: Board::new(black, white), player, limits, hash_bits })
}

/// Writes a `MoveResponse` (C#: `EngineProtocol.WriteResult`).
pub fn write_result(result: &SearchResult) -> String {
    let square = result.mv.0.map_or(-1, |square| square.index() as i32);
    format!(
        r#"{{"Square":{square},"Score":{},"Kind":{},"Depth":{},"Nodes":{},"Evaluations":{},"ElapsedTicks":{}}}"#,
        result.score,
        result.kind as u8,
        result.depth,
        result.nodes,
        result.evaluations,
        ticks(result.elapsed)
    )
}

/// Writes a `ProgressReport` (C#: `EngineProtocol.WriteProgress`).
pub fn write_progress(info: &SearchInfo) -> String {
    let mut json = String::with_capacity(160);
    let best = info.best_move.map_or(-1, |square| square.index() as i32);
    // Writing to a String cannot fail.
    let _ = write!(
        json,
        r#"{{"Depth":{},"CurrentMove":{},"BestMove":{best},"Score":{},"Kind":{},"Nodes":{},"Evaluations":{},"ElapsedTicks":{}}}"#,
        info.depth,
        info.current_move.index(),
        info.score,
        info.kind as u8,
        info.nodes,
        info.evaluations,
        ticks(info.elapsed)
    );
    json
}

/// .NET ticks (100 ns), as `TimeSpan.Ticks`.
fn ticks(duration: Duration) -> u64 {
    u64::try_from(duration.as_nanos() / 100).unwrap_or(u64::MAX)
}

/// A value of a flat object: what the requests contain.
#[derive(Debug)]
enum Value<'a> {
    /// A number as written (integers only are read).
    Number(&'a str),
    Bool,
    Null,
    Text,
}

impl Value<'_> {
    fn integer(&self, name: &str) -> Result<i64, String> {
        match self {
            Value::Number(text) => text.parse().map_err(|_| format!("{name} is not an integer: {text}.")),
            _ => Err(format!("{name} is not a number.")),
        }
    }

    fn unsigned(&self, name: &str) -> Result<u64, String> {
        match self {
            Value::Number(text) => text.parse().map_err(|_| format!("{name} is not a 64-bit board: {text}.")),
            _ => Err(format!("{name} is not a number.")),
        }
    }
}

/// Reads one JSON object whose values are numbers, booleans, null or strings (no nested objects or arrays, which
/// the protocol does not use).
struct Reader<'a> {
    text: &'a str,
    at: usize,
}

impl<'a> Reader<'a> {
    fn new(text: &'a str) -> Reader<'a> {
        Reader { text, at: 0 }
    }

    fn object(mut self) -> Result<Vec<(&'a str, Value<'a>)>, String> {
        let mut members = Vec::new();
        self.expect(b'{')?;
        if self.peek() == Some(b'}') {
            self.at += 1;
        } else {
            loop {
                let name = self.string()?;
                self.expect(b':')?;
                members.push((name, self.value()?));
                match self.next() {
                    Some(b',') => {}
                    Some(b'}') => break,
                    _ => return Err(self.error("',' or '}'")),
                }
            }
        }

        if self.peek().is_some() {
            return Err(self.error("the end"));
        }
        Ok(members)
    }

    fn value(&mut self) -> Result<Value<'a>, String> {
        match self.peek() {
            Some(b'"') => self.string().map(|_| Value::Text),
            Some(b't') => self.word("true").map(|_| Value::Bool),
            Some(b'f') => self.word("false").map(|_| Value::Bool),
            Some(b'n') => self.word("null").map(|_| Value::Null),
            Some(b'-' | b'0'..=b'9') => {
                let start = self.at;
                let bytes = self.text.as_bytes();
                while self.at < bytes.len() && matches!(bytes[self.at], b'-' | b'+' | b'.' | b'e' | b'E' | b'0'..=b'9')
                {
                    self.at += 1;
                }
                Ok(Value::Number(&self.text[start..self.at]))
            }
            _ => Err(self.error("a value")),
        }
    }

    /// A string without escapes (the protocol's names); a string with escapes is an error.
    fn string(&mut self) -> Result<&'a str, String> {
        self.expect(b'"')?;
        let start = self.at;
        let end = self.text[start..].find('"').map(|offset| start + offset).ok_or_else(|| self.error("'\"'"))?;
        let text = &self.text[start..end];
        if text.contains('\\') {
            return Err(self.error("a string without escapes"));
        }
        self.at = end + 1;
        Ok(text)
    }

    fn word(&mut self, word: &str) -> Result<(), String> {
        if self.text[self.at..].starts_with(word) {
            self.at += word.len();
            Ok(())
        } else {
            Err(self.error(word))
        }
    }

    fn expect(&mut self, byte: u8) -> Result<(), String> {
        if self.next() == Some(byte) { Ok(()) } else { Err(self.error(&format!("'{}'", byte as char))) }
    }

    /// The next byte after white space, consumed.
    fn next(&mut self) -> Option<u8> {
        let byte = self.peek()?;
        self.at += 1;
        Some(byte)
    }

    /// The next byte after white space (which is skipped).
    fn peek(&mut self) -> Option<u8> {
        let bytes = self.text.as_bytes();
        while self.at < bytes.len() && bytes[self.at].is_ascii_whitespace() {
            self.at += 1;
        }
        bytes.get(self.at).copied()
    }

    fn error(&self, expected: &str) -> String {
        format!("Not a valid engine request: expected {expected} at character {}.", self.at)
    }
}

#[cfg(test)]
mod tests {
    use stello_core::{Move, ScoreKind, Square};

    use super::*;

    #[test]
    fn reads_the_requests_of_engine_protocol() {
        // As System.Text.Json writes them: the boards above 2^53 must stay exact.
        let json = r#"{"Black":18446462598732840960,"White":34628173824,"Player":1,"Mode":2,"Depth":0,"TimeTicks":-15005000,"UseOpeningBook":true,"HashBits":20}"#;
        let request = read_request(json).unwrap();
        assert_eq!(request.board, Board::new(18446462598732840960, 34628173824));
        assert_eq!(request.player, Player::White);
        assert_eq!(request.limits, SearchLimits::time_per_game_ms(-1500));
        assert_eq!(request.hash_bits, 20);

        let json =
            r#" { "Black" : 1 , "White" : 2, "Player": 0, "Mode": 0, "Depth": 12, "TimeTicks": 0, "Extra": "x" } "#;
        let request = read_request(json).unwrap();
        assert_eq!(request.limits, SearchLimits::fixed_depth(12));
        assert_eq!(request.hash_bits, DEFAULT_HASH_BITS);

        let json = r#"{"Black":1,"White":2,"Player":0,"Mode":1,"Depth":0,"TimeTicks":5009999}"#;
        assert_eq!(read_request(json).unwrap().limits, SearchLimits::time_per_move_ms(500));
        let json = r#"{"Black":1,"White":2,"Player":0,"Mode":3,"Depth":0,"TimeTicks":0,"UseOpeningBook":false}"#;
        assert_eq!(read_request(json).unwrap().limits, SearchLimits::SOLVE);
        let json = r#"{"Black":1,"White":2,"Player":0,"Mode":3,"Depth":0,"TimeTicks":0,"HashBits":19,"Threads":8}"#;
        assert_eq!(read_request(json).unwrap().limits, SearchLimits::SOLVE.with_threads(8).unwrap());
    }

    #[test]
    fn rejects_requests_that_are_not_valid() {
        for json in [
            "",
            "{",
            "[]",
            r#"{"Black":1,"White":2,"Player":0,"Mode":0,"Depth":4}x"#,
            r#"{"Black":1,"White":3,"Player":0,"Mode":0,"Depth":4}"#,
            r#"{"Black":1,"White":2,"Player":2,"Mode":0,"Depth":4}"#,
            r#"{"Black":1,"White":2,"Player":0,"Mode":4,"Depth":4}"#,
            r#"{"Black":1,"White":2,"Player":0,"Mode":0,"Depth":61}"#,
            r#"{"Black":1,"White":2,"Player":0,"Mode":0,"Depth":4,"HashBits":9}"#,
            r#"{"Black":1,"White":2,"Player":0,"Mode":0,"Depth":4,"Threads":0}"#,
            r#"{"Black":1,"White":2,"Player":0,"Mode":0,"Depth":4,"Threads":257}"#,
            r#"{"Black":-1,"White":2,"Player":0,"Mode":0,"Depth":4}"#,
            r#"{"Black":"1","White":2,"Player":0,"Mode":0,"Depth":4}"#,
            r#"{"White":2,"Player":0,"Mode":0,"Depth":4}"#,
            r#"{"Black\"":1,"White":2,"Player":0,"Mode":0,"Depth":4}"#,
        ] {
            assert!(read_request(json).is_err(), "{json}");
        }
    }

    #[test]
    fn writes_results_and_progress_as_engine_protocol() {
        let result = SearchResult {
            mv: Move(Some(Square::new(37))),
            score: -12,
            kind: ScoreKind::Exact,
            depth: 22,
            nodes: 123_456_789,
            evaluations: 42,
            elapsed: Duration::from_micros(1_234_567),
        };
        assert_eq!(
            write_result(&result),
            r#"{"Square":37,"Score":-12,"Kind":3,"Depth":22,"Nodes":123456789,"Evaluations":42,"ElapsedTicks":12345670}"#
        );
        assert!(write_result(&SearchResult { mv: Move::PASS, ..result }).starts_with(r#"{"Square":-1,"#));

        let info = SearchInfo {
            depth: 9,
            current_move: Square::new(19),
            best_move: None,
            score: 150,
            kind: ScoreKind::Heuristic,
            nodes: 1000,
            evaluations: 900,
            elapsed: Duration::from_millis(250),
        };
        assert_eq!(
            write_progress(&info),
            r#"{"Depth":9,"CurrentMove":19,"BestMove":-1,"Score":150,"Kind":1,"Nodes":1000,"Evaluations":900,"ElapsedTicks":2500000}"#
        );
    }
}
