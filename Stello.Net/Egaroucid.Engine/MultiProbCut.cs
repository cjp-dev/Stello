// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/multi_probcut.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana,
// GPL-3.0-or-later.

namespace Stello.Egaroucid;

/// <summary>
/// Multi-ProbCut (multi_probcut.hpp): a shallow search predicts whether the deep one would fail high or low. The error
/// of the prediction is a fitted function of the number of discs and the two depths, times a factor per selectivity.
/// </summary>
internal static class MultiProbCut
{
    public const int USE_MPC_MIN_DEPTH = 3;
    private const int MPC_ERROR0_OFFSET = 4;

    private static ReadOnlySpan<double> SelectivityMpct => [1.13, 1.55, 1.81, 2.32, 2.57, 3.29, 9.99];

    private const double ProbcutA = 0.7308488452189136;
    private const double ProbcutB = -4.5708322989025865;
    private const double ProbcutC = 1.096319765006055;
    private const double ProbcutD = -0.8362251801219095;
    private const double ProbcutE = 4.610017383697701;
    private const double ProbcutF = 3.818582623595395;
    private const double ProbcutG = 1.7775013664098447;

    private const int Depths = Common.HW2 - 3;

    /// <summary>mpc_error[level][n_discs][shallow depth][deep depth], precomputed as in C++ (<c>mpc_init</c>).</summary>
    public static readonly int[] MpcError = CreateTable();

    public static int Error(int mpcLevel, int nDiscs, int depth1, int depth2) =>
        MpcError[((mpcLevel * (Common.HW2 + 1) + nDiscs) * Depths + depth1) * Depths + depth2];

    public static double ProbcutSigma(int nDiscs, int depth1, int depth2)
    {
        double res = ProbcutA * (nDiscs / 64.0) + ProbcutB * (depth1 / 60.0) + ProbcutC * (depth2 / 60.0);
        res = ProbcutD * res * res * res + ProbcutE * res * res + ProbcutF * res + ProbcutG;
        return res;
    }

    private static int[] CreateTable()
    {
        var table = new int[Level.N_SELECTIVITY_LEVEL * (Common.HW2 + 1) * Depths * Depths];
        int i = 0;
        for (int mpcLevel = 0; mpcLevel < Level.N_SELECTIVITY_LEVEL; ++mpcLevel)
        {
            for (int nDiscs = 0; nDiscs < Common.HW2 + 1; ++nDiscs)
            {
                for (int depth1 = 0; depth1 < Depths; ++depth1)
                {
                    for (int depth2 = 0; depth2 < Depths; ++depth2)
                    {
                        table[i++] = (int)Math.Ceiling(SelectivityMpct[mpcLevel] * ProbcutSigma(nDiscs, depth1, depth2));
                    }
                }
            }
        }

        return table;
    }

    /// <summary>Tries to cut the node off with a shallow search (<c>mpc</c>); true with the value in <paramref name="v"/>.</summary>
    public static bool Mpc(Search search, int alpha, int beta, int depth, ulong legal, bool isEndSearch, ref int v, SearchingFlag searching)
    {
        int searchDepth = isEndSearch
            ? ((depth / 3) & 0b11111110) + (depth & 1)
            : ((depth / 2) & 0b11111110) + (depth & 1);
        int d0value = MidSearch.MidEvaluateDiff(search);
        if (isEndSearch)
        {
            if ((alpha & 1) == 0)
            {
                alpha += 1;
            }

            if ((beta & 1) == 0)
            {
                beta -= 1;
            }
        }

        if (searchDepth == 0)
        {
            int error = Error(search.MpcLevel, search.NDiscs, 0, depth);
            if (d0value >= beta + error)
            {
                v = beta;
                if (isEndSearch)
                {
                    v += beta & 1;
                }

                return true;
            }

            if (d0value <= alpha - error)
            {
                v = alpha;
                if (isEndSearch)
                {
                    v -= alpha & 1;
                }

                return true;
            }
        }
        else
        {
            int mpcLevel = search.MpcLevel;
            int errorSearch = Error(mpcLevel, search.NDiscs, searchDepth, depth);
            int error0 = Math.Max(1, errorSearch - MPC_ERROR0_OFFSET);
            search.MpcLevel = Level.MPC_100;
            if (d0value >= beta + error0)
            {
                int pcBeta = beta + errorSearch;
                if (pcBeta <= Common.SCORE_MAX)
                {
                    if (MidSearch.NegaAlphaOrderingNws(search, pcBeta - 1, searchDepth, false, legal, false, searching) >= pcBeta)
                    {
                        v = beta;
                        if (isEndSearch)
                        {
                            v += beta & 1;
                        }

                        search.MpcLevel = mpcLevel;
                        return true;
                    }
                }
            }

            if (d0value <= alpha - error0)
            {
                int pcAlpha = alpha - errorSearch;
                if (pcAlpha >= -Common.SCORE_MAX)
                {
                    if (MidSearch.NegaAlphaOrderingNws(search, pcAlpha, searchDepth, false, legal, false, searching) <= pcAlpha)
                    {
                        v = alpha;
                        if (isEndSearch)
                        {
                            v -= alpha & 1;
                        }

                        search.MpcLevel = mpcLevel;
                        return true;
                    }
                }
            }

            search.MpcLevel = mpcLevel;
        }

        return false;
    }
}
