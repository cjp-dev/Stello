//! Parity with the C# Stello 2.0 engine: the fixtures in tests/data are written by
//! `Stello.Benchmark parity-data` from `Stello.Engine.V2` and the shared `Stello.Engine` types. Every value must be
//! equal.

use stello_core::eval::{self, EvalWeights, PatternSet};
use stello_core::{Board, Player, Square, bits, board, endgame};

const FNV_OFFSET: u64 = 14_695_981_039_346_656_037;
const FNV_PRIME: u64 = 1_099_511_628_211;

fn fnv32(hash: u64, value: i32) -> u64 {
    value.to_le_bytes().iter().fold(hash, |h, &b| (h ^ b as u64).wrapping_mul(FNV_PRIME))
}

fn fnv16(hash: u64, value: i16) -> u64 {
    value.to_le_bytes().iter().fold(hash, |h, &b| (h ^ b as u64).wrapping_mul(FNV_PRIME))
}

fn hex(text: &str) -> u64 {
    u64::from_str_radix(text, 16).unwrap_or_else(|_| panic!("not hex: {text}"))
}

fn int(text: &str) -> i32 {
    text.parse().unwrap_or_else(|_| panic!("not a number: {text}"))
}

fn lines(name: &str) -> Vec<String> {
    let path = format!("{}/tests/data/{name}", env!("CARGO_MANIFEST_DIR"));
    std::fs::read_to_string(&path)
        .unwrap_or_else(|e| panic!("{path}: {e}"))
        .lines()
        .filter(|line| !line.is_empty() && !line.starts_with('#'))
        .map(str::to_string)
        .collect()
}

struct Fixture {
    id: String,
    board: Board,
    player: Player,
}

impl Fixture {
    fn own(&self) -> u64 {
        self.board.discs(self.player)
    }

    fn opponent(&self) -> u64 {
        self.board.discs(self.player.opponent())
    }
}

fn parse_position(fields: &[&str]) -> Fixture {
    let player = match fields[3] {
        "X" => Player::Black,
        "O" => Player::White,
        other => panic!("side {other}"),
    };
    Fixture { id: fields[0].to_string(), board: Board::new(hex(fields[1]), hex(fields[2])), player }
}

#[test]
fn positions_match_stello_2() {
    let weights = EvalWeights::built_in();
    let lines = lines("positions.txt");
    assert!(lines.len() > 5000, "the fixture has {} positions", lines.len());
    let mut fixtures: Vec<(Fixture, Vec<Board>)> = Vec::with_capacity(lines.len());
    let (mut moves_checked, mut passes, mut finished) = (0usize, 0usize, 0usize);

    for line in &lines {
        let fields: Vec<&str> = line.split(' ').collect();
        let position = parse_position(&fields);
        let id = &position.id;
        let (board, player) = (position.board, position.player);
        let (own, opponent) = (position.own(), position.opponent());
        let empty = board.empty();

        let legal = hex(fields[4]);
        assert_eq!(bits::legal_moves(own, opponent), legal, "{id}: Bitboards.LegalMoves");
        assert_eq!(endgame::bits::legal_moves(own, opponent), legal, "{id}: EndgameBits.LegalMoves");
        assert_eq!(board.legal_moves(player), legal, "{id}: Board.LegalMoves");
        assert_eq!(board.legal_moves(player.opponent()), hex(fields[5]), "{id}: opponent's Board.LegalMoves");
        assert_eq!(bits::potential_mobility(own, opponent), int(fields[6]), "{id}: Bitboards.PotentialMobility");
        assert_eq!(
            endgame::bits::potential_mobility(own, empty),
            int(fields[7]),
            "{id}: EndgameBits.PotentialMobility own"
        );
        assert_eq!(
            endgame::bits::potential_mobility(opponent, empty),
            int(fields[8]),
            "{id}: EndgameBits.PotentialMobility opp"
        );
        assert_eq!(endgame::bits::lone_empties(own, opponent), hex(fields[9]), "{id}: LoneEmpties");
        assert_eq!(endgame::bits::parity(empty), int(fields[10]), "{id}: Parity");
        assert_eq!(weights.stage_of((own | opponent).count_ones()), int(fields[11]) as usize, "{id}: StageOf");
        assert_eq!(weights.evaluate(own, opponent), int(fields[12]), "{id}: Evaluate");
        assert_eq!(eval::pattern_score(weights, own, opponent), int(fields[13]), "{id}: PatternScore");
        assert_eq!(eval::disc_score(weights, own, opponent), int(fields[14]), "{id}: OrderingValue");

        let move_fields = &fields[15..];
        let squares: Vec<Square> = Square::in_mask(legal).collect();
        assert_eq!(move_fields.len(), squares.len(), "{id}: number of moves");
        let mut children = Vec::with_capacity(squares.len());
        for (field, &square) in move_fields.iter().zip(&squares) {
            let parts: Vec<&str> = field.split(':').collect();
            assert_eq!(int(parts[0]), square.index() as i32, "{id}: move order");
            let flips = hex(parts[1]);
            let s = square.index() as u32;
            assert_eq!(bits::flips(own, opponent, s), flips, "{id} {square}: Bitboards.Flips");
            assert_eq!(endgame::bits::flips(own, opponent, s), flips, "{id} {square}: EndgameBits.Flips");
            assert_eq!(board.flips(player, square), flips, "{id} {square}: Board.Flips");
            assert!(flips != 0, "{id} {square}: a legal move flips");

            // C# Board.Play: the disc placed and the flips turned (the C# command checks its Play against this).
            let placed = flips | square.bit();
            let expected = match player {
                Player::Black => Board::new(board.black() | placed, board.white() & !flips),
                Player::White => Board::new(board.black() & !flips, board.white() | placed),
            };
            let child = board.play(player, square).expect("a legal move");
            assert_eq!(child, expected, "{id} {square}: Board.Play");
            assert_eq!(board.play_unchecked(player, s), expected, "{id} {square}: SearchEngine.Play");
            assert_eq!(
                weights.evaluate(child.discs(player.opponent()), child.discs(player)),
                int(parts[2]),
                "{id} {square}: Evaluate after the move"
            );
            children.push(child);
            moves_checked += 1;
        }

        // Every empty square that is not a legal move flips nothing and cannot be played.
        for square in Square::in_mask(empty & !legal) {
            assert_eq!(board.flips(player, square), 0, "{id} {square}: Board.Flips of an illegal move");
            assert_eq!(board.play(player, square), None, "{id} {square}: Board.Play of an illegal move");
        }

        let game_over = legal == 0 && hex(fields[5]) == 0;
        assert_eq!(board.is_game_over(), game_over, "{id}: IsGameOver");
        if game_over {
            finished += 1;
        } else if legal == 0 {
            passes += 1;
        }

        fixtures.push((position, children));
    }

    // The lines of one game follow each other: each is a move (Board.Play) or a pass in the line before.
    let mut successions = 0;
    for pair in fixtures.windows(2) {
        let ((before, children), (after, _)) = (&pair[0], &pair[1]);
        let (Some((game, ply)), Some((next_game, next_ply))) = (before.id.rsplit_once('.'), after.id.rsplit_once('.'))
        else {
            continue;
        };
        if game != next_game || int(next_ply) != int(ply) + 1 {
            continue;
        }

        assert_eq!(after.player, before.player.opponent(), "{}: the side to move changes", after.id);
        if children.is_empty() {
            assert_eq!(after.board, before.board, "{}: a pass keeps the board", after.id);
        } else {
            assert!(children.contains(&after.board), "{}: the position after one of the moves", after.id);
        }
        successions += 1;
    }

    assert!(passes >= 20 && finished >= 100, "{passes} passes and {finished} finished games in the fixture");
    assert!(moves_checked > 30_000 && successions > 5000, "{moves_checked} moves, {successions} successions");
}

#[test]
fn pattern_sets_match_stello_2() {
    let positions: Vec<Fixture> =
        lines("positions.txt").iter().map(|line| parse_position(&line.split(' ').collect::<Vec<_>>())).collect();
    let lines = lines("patterns.txt");
    let mut checked_sets = 0;
    let mut i = 0;
    while i < lines.len() {
        let fields: Vec<&str> = lines[i].split(' ').collect();
        i += 1;
        match fields[0] {
            "set" => {
                let set = PatternSet::by_name(fields[1]).unwrap_or_else(|| panic!("set {}", fields[1]));
                assert_eq!(set.feature_count(), int(fields[2]) as usize, "{}: FeatureCount", fields[1]);
                assert_eq!(set.raw_size(), int(fields[3]) as usize, "{}: RawSize", fields[1]);
                let symmetries: Vec<usize> = fields[4].split(',').map(|s| int(s) as usize).collect();
                let ours: Vec<usize> = (0..set.feature_count()).map(|f| set.symmetry_of(f)).collect();
                assert_eq!(ours, symmetries, "{}: the features' symmetries", fields[1]);

                let mut hash = FNV_OFFSET;
                let mut indices = vec![0u32; set.feature_count()];
                for position in &positions {
                    set.indices(position.own(), position.opponent(), &mut indices);
                    hash = indices.iter().fold(hash, |h, &index| fnv32(h, index as i32));
                }
                assert_eq!(hash, hex(fields[5]), "{}: the indices of all positions", fields[1]);

                for shape in set.shapes() {
                    let fields: Vec<&str> = lines[i].split(' ').collect();
                    i += 1;
                    assert_eq!(fields[0], "shape");
                    assert_eq!(shape.name(), fields[1], "{}: shape order", set.name());
                    assert_eq!(shape.size(), int(fields[2]) as usize, "{}: Size", shape.name());
                    let placements: Vec<usize> = fields[3].split(',').map(|s| int(s) as usize).collect();
                    assert_eq!(shape.placements(), placements.as_slice(), "{}: Placements", shape.name());
                    assert_eq!(
                        shape.representatives().len(),
                        int(fields[4]) as usize,
                        "{}: Representatives",
                        shape.name()
                    );
                    let fold = shape.fold().iter().fold(FNV_OFFSET, |h, &f| fnv32(h, f as i32));
                    assert_eq!(fold, hex(fields[5]), "{}: Fold", shape.name());
                }
                checked_sets += 1;
            }
            "weights" => {
                let weights = EvalWeights::built_in();
                assert_eq!(weights.set().name(), fields[1]);
                assert_eq!(weights.stages(), int(fields[2]) as usize, "Stages");
                assert_eq!(weights.units_per_disc(), int(fields[3]), "UnitsPerDisc");
                let stage_of: Vec<usize> = fields[4].split(',').map(|s| int(s) as usize).collect();
                assert_eq!((0..=64).map(|d| weights.stage_of(d)).collect::<Vec<_>>(), stage_of, "StageOf");
                let mut hash = FNV_OFFSET;
                for stage in 0..weights.stages() {
                    hash = weights.stage_weights(stage).iter().fold(hash, |h, &w| fnv16(h, w));
                }
                assert_eq!(hash, hex(fields[5]), "all weights");
            }
            other => panic!("unknown line {other}"),
        }
    }

    assert_eq!(checked_sets, 3);
}

#[test]
fn perft_matches_stello_2() {
    // Stello.Engine.Tests.PerftTests.
    let expected = [4u64, 12, 56, 244, 1396, 8200, 55092, 390_216];
    for (depth, &leaves) in expected.iter().enumerate() {
        assert_eq!(board::perft(&Board::INITIAL, Player::Black, depth as u32 + 1), leaves, "perft({})", depth + 1);
    }

    let lines = lines("perft.txt");
    assert!(lines.len() >= 8);
    for line in lines {
        let fields: Vec<&str> = line.split(' ').collect();
        let position = parse_position(&fields);
        let depth = int(fields[4]) as u32;
        let leaves: u64 = fields[5].parse().unwrap();
        assert_eq!(board::perft(&position.board, position.player, depth), leaves, "{} perft({depth})", position.id);
    }
}
