// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/evaluate_common.hpp and evaluate_generic.hpp (https://github.com/Nyanyan/Egaroucid),
// © Takuto Yamana, GPL-3.0-or-later.

using System.Runtime.CompilerServices;

namespace Stello.Egaroucid;

/// <summary>
/// The pattern evaluation (evaluate_generic.hpp). Each phase (number of discs − 4, 60 phases) has one table per pattern
/// and a table indexed by the side to move's disc count. The sum is in 1/32 discs and is rounded to whole discs.
/// </summary>
/// <remarks>
/// The C++ generic code keeps a second, colour-swapped copy of every table (<c>pattern_arr[1]</c>) for the positions
/// where the side to move is not the side the features were computed for. The port keeps one table and instead keeps
/// the features in both colourings (<see cref="EvalSearch"/>); the values are the same.
/// </remarks>
internal static class Evaluate
{
    public const int N_PHASES = 60;
    public const int MAX_STONE_NUM = 65;
    public const int STEP = 32;
    public const int STEP_2 = 16;
    private const int N_ZEROS_PLUS = 1 << 12;

    /// <summary>The number of pattern weights per phase: 612,360.</summary>
    public static readonly int PhaseSize = SumPatternSizes();

    /// <summary>For each feature, the offset of its pattern's table within a phase.</summary>
    private static readonly int[] FeatureOffset = CreateFeatureOffsets();

    // Pattern weights: [phase * PhaseSize + FeatureOffset(pattern) + index]; disc-count weights: [phase * 65 + discs].
    private static short[] _patternWeights = [];
    private static short[] _discCountWeights = [];

    // The move-ordering evaluation of the endgame: patterns 8..11, one phase: [(pattern − 8) * 59049 + index].
    private static short[] _moveOrderingEndWeights = [];

    public static bool IsLoaded => _patternWeights.Length > 0;

    /// <summary>Loads <c>eval.egev2</c> and <c>eval_move_ordering_end.egev</c> (evaluate_generic.hpp <c>evaluate_init</c>).</summary>
    /// <exception cref="IOException">A file is missing or does not have the expected size.</exception>
    public static void Load(string evalFile, string moveOrderingEndFile)
    {
        short[] values = LoadUnzipEgev2(evalFile);
        long expected = (long)(PhaseSize + MAX_STONE_NUM) * N_PHASES;
        if (values.Length < expected)
        {
            throw new IOException($"{evalFile} has {values.Length} weights, expected {expected}");
        }

        var patterns = new short[PhaseSize * N_PHASES];
        var discCounts = new short[MAX_STONE_NUM * N_PHASES];
        int source = 0;
        for (int phase = 0; phase < N_PHASES; ++phase)
        {
            Array.Copy(values, source, patterns, phase * PhaseSize, PhaseSize);
            source += PhaseSize;
            Array.Copy(values, source, discCounts, phase * MAX_STONE_NUM, MAX_STONE_NUM);
            source += MAX_STONE_NUM;
        }

        byte[] bytes = File.ReadAllBytes(moveOrderingEndFile);
        int moveOrderingEndSize = Patterns.N_PATTERNS_MO_END * Patterns.MAX_EVALUATE_IDX;
        if (bytes.Length < moveOrderingEndSize * 2)
        {
            throw new IOException($"{moveOrderingEndFile} is too short");
        }

        var moveOrderingEnd = new short[moveOrderingEndSize];
        Buffer.BlockCopy(bytes, 0, moveOrderingEnd, 0, moveOrderingEndSize * 2);

        _patternWeights = patterns;
        _discCountWeights = discCounts;
        _moveOrderingEndWeights = moveOrderingEnd;
    }

    /// <summary>
    /// Reads an <c>.egev2</c> file (evaluate_common.hpp <c>load_unzip_egev2</c>): an int32 count, then that many int16
    /// values, where a value of 4096 or more stands for (value − 4096) zeros.
    /// </summary>
    public static short[] LoadUnzipEgev2(string file)
    {
        byte[] bytes = File.ReadAllBytes(file);
        if (bytes.Length < 4)
        {
            throw new IOException($"{file} is too short");
        }

        int nZipped = BitConverter.ToInt32(bytes, 0);
        if (nZipped < 0 || bytes.Length < 4 + 2L * nZipped)
        {
            throw new IOException($"{file} is too short for {nZipped} values");
        }

        ReadOnlySpan<short> zipped = System.Runtime.InteropServices.MemoryMarshal.Cast<byte, short>(bytes.AsSpan(4, 2 * nZipped));
        long nParams = 0;
        foreach (short value in zipped)
        {
            nParams += value >= N_ZEROS_PLUS ? value - N_ZEROS_PLUS : 1;
        }

        var result = new short[nParams];
        int index = 0;
        foreach (short value in zipped)
        {
            if (value >= N_ZEROS_PLUS)
            {
                index += value - N_ZEROS_PLUS;
            }
            else
            {
                result[index++] = value;
            }
        }

        return result;
    }

    /// <summary>The evaluation of a position from scratch, in discs for the side to move (evaluate_generic.hpp <c>mid_evaluate</c>).</summary>
    public static int MidEvaluate(in Board board)
    {
        var eval = new EvalSearch();
        eval.Init(board);
        return MidEvaluateDiff(eval, board.NDiscs(), board.CountPlayer());
    }

    /// <summary>
    /// The evaluation from the features in <paramref name="eval"/> (evaluate_generic.hpp <c>mid_evaluate_diff</c>), for a
    /// position with <paramref name="nDiscs"/> discs of which <paramref name="playerDiscs"/> are the side to move's.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int MidEvaluateDiff(EvalSearch eval, int nDiscs, int playerDiscs)
    {
        int phase = nDiscs - 4;
        int res = CalcPattern(phase, eval) + _discCountWeights[phase * MAX_STONE_NUM + playerDiscs];
        res += res >= 0 ? STEP_2 : -STEP_2;
        res /= STEP;
        return Math.Clamp(res, -Common.SCORE_MAX, Common.SCORE_MAX);
    }

    /// <summary>The move-ordering evaluation of the endgame (evaluate_generic.hpp <c>mid_evaluate_move_ordering_end</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int MidEvaluateMoveOrderingEnd(EvalSearch eval)
    {
        ReadOnlySpan<ushort> features = eval.CurrentFeatures;
        short[] weights = _moveOrderingEndWeights;
        int res = 0;
        for (int i = Patterns.EVAL_IDX_START_MOVE_ORDERING_END; i < Patterns.EVAL_IDX_END_MOVE_ORDERING_END; ++i)
        {
            int pattern = Patterns.FeatureToPattern(i) - Patterns.EVAL_FEATURE_START_MOVE_ORDERING_END;
            res += weights[pattern * Patterns.MAX_EVALUATE_IDX + features[i]];
        }

        res += res >= 0 ? STEP_2 : -STEP_2;
        res /= STEP;
        return res;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static int CalcPattern(int phase, EvalSearch eval)
    {
        ReadOnlySpan<ushort> features = eval.CurrentFeatures;
        ReadOnlySpan<short> weights = _patternWeights.AsSpan(phase * PhaseSize, PhaseSize);
        ReadOnlySpan<int> offsets = FeatureOffset;
        int res = 0;
        for (int i = 0; i < Patterns.N_PATTERN_FEATURES; ++i)
        {
            res += weights[offsets[i] + features[i]];
        }

        return res;
    }

    private static int SumPatternSizes()
    {
        int size = 0;
        foreach (int cells in Patterns.PatternSizes)
        {
            size += Patterns.Pow3[cells];
        }

        return size;
    }

    private static int[] CreateFeatureOffsets()
    {
        var patternOffset = new int[Patterns.N_PATTERNS];
        for (int pattern = 1; pattern < Patterns.N_PATTERNS; pattern++)
        {
            patternOffset[pattern] = patternOffset[pattern - 1] + Patterns.Pow3[Patterns.PatternSizes[pattern - 1]];
        }

        var offsets = new int[Patterns.N_PATTERN_FEATURES];
        for (int feature = 0; feature < Patterns.N_PATTERN_FEATURES; feature++)
        {
            offsets[feature] = patternOffset[Patterns.FeatureToPattern(feature)];
        }

        return offsets;
    }
}

/// <summary>
/// The pattern features along the current line of the search, updated move by move (evaluate_common.hpp
/// <c>Eval_search</c> and evaluate_generic.hpp <c>eval_move</c> / <c>eval_undo</c> / <c>eval_pass</c>).
/// </summary>
/// <remarks>
/// As in C++, <c>_featuresA</c> holds the features with the digits of the position the search started from (0 = the
/// side to move there) and <c>_reversed</c> says whether the side to move is now the other colour. The port also keeps
/// <c>_featuresB</c>, the same features with 0 and 1 swapped, so the evaluation can read the view of the side to move
/// from one table: A when not reversed, B when reversed.
/// </remarks>
internal sealed class EvalSearch
{
    // C++ has 60 slots (HW2 - 4); the search never updates features for the last move. One more slot lets a whole
    // game be played move by move (used by the tests).
    private const int Depth = Common.HW2 - 4 + 1;
    private const int N = Patterns.N_PATTERN_FEATURES;

    private readonly ushort[] _featuresA = new ushort[Depth * N];
    private readonly ushort[] _featuresB = new ushort[Depth * N];
    private readonly bool[] _reversed = new bool[Depth];
    private int _featureIdx;

    public int FeatureIdx => _featureIdx;

    public bool Reversed => _reversed[_featureIdx];

    /// <summary>The 64 features from the side to move's view, as the evaluation reads them.</summary>
    public ReadOnlySpan<ushort> CurrentFeatures =>
        (_reversed[_featureIdx] ? _featuresB : _featuresA).AsSpan(_featureIdx * N, N);

    /// <summary>The features as C++ stores them (the view of the side that moved first), for tests.</summary>
    public ReadOnlySpan<ushort> RawFeatures => _featuresA.AsSpan(_featureIdx * N, N);

    /// <summary>Computes the features of <paramref name="board"/> from scratch (evaluate_generic.hpp <c>calc_eval_features</c>).</summary>
    public void Init(in Board board)
    {
        Span<ushort> a = _featuresA.AsSpan(0, N);
        Span<ushort> b = _featuresB.AsSpan(0, N);
        for (int i = 0; i < N; ++i)
        {
            int indexA = 0, indexB = 0;
            foreach (int cell in Patterns.FeatureToCoord[i])
            {
                ulong bit = 1UL << cell;
                int digit = (board.Player & bit) != 0 ? 0 : (board.Opponent & bit) != 0 ? 1 : 2;
                indexA = indexA * 3 + digit;
                indexB = indexB * 3 + (digit == 2 ? 2 : 1 - digit);
            }

            a[i] = (ushort)indexA;
            b[i] = (ushort)indexB;
        }

        _reversed[0] = false;
        _featureIdx = 0;
    }

    /// <summary>Plays a move: the new features go one step deeper (evaluate_generic.hpp <c>eval_move</c>).</summary>
    public void Move(in Flip flip) => Move(flip, Patterns.CoordToFeature, 0, N);

    /// <summary>Plays a move updating only the move-ordering features of the endgame (<c>eval_move_endsearch</c>).</summary>
    public void MoveEndsearch(in Flip flip) =>
        Move(flip, Patterns.CoordToFeatureMoveOrderingEnd, Patterns.EVAL_IDX_START_MOVE_ORDERING_END,
            Patterns.EVAL_IDX_END_MOVE_ORDERING_END - Patterns.EVAL_IDX_START_MOVE_ORDERING_END);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void Undo() => --_featureIdx;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void Pass() => _reversed[_featureIdx] = !_reversed[_featureIdx];

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void Move(in Flip flip, (int Feature, int Power)[][] coordToFeature, int start, int count)
    {
        int from = _featureIdx * N, to = from + N;
        Array.Copy(_featuresA, from + start, _featuresA, to + start, count);
        Array.Copy(_featuresB, from + start, _featuresB, to + start, count);
        Span<ushort> a = _featuresA.AsSpan(to, N);
        Span<ushort> b = _featuresB.AsSpan(to, N);

        // The mover's digit in A is 0 when not reversed, 1 when reversed; in B the other way round.
        // Placing a disc changes an empty square (2) to the mover's digit; flipping changes the other digit to it.
        bool reversed = _reversed[_featureIdx];
        int placedA = reversed ? 1 : 2, placedB = reversed ? 2 : 1;
        int flippedA = reversed ? -1 : 1, flippedB = -flippedA;
        foreach ((int feature, int power) in coordToFeature[flip.Pos])
        {
            a[feature] -= (ushort)(placedA * power);
            b[feature] -= (ushort)(placedB * power);
        }

        ulong f = flip.Flipped;
        for (int cell = Bits.FirstBit(ref f); f != 0; cell = Bits.NextBit(ref f))
        {
            foreach ((int feature, int power) in coordToFeature[cell])
            {
                a[feature] = (ushort)(a[feature] - flippedA * power);
                b[feature] = (ushort)(b[feature] - flippedB * power);
            }
        }

        _reversed[_featureIdx + 1] = !reversed;
        ++_featureIdx;
    }
}
