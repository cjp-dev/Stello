// SPDX-License-Identifier: GPL-3.0-only
//
// Reference driver for the C# port of Egaroucid (Stello.Net/Egaroucid.Engine).
//
// It includes Egaroucid's engine unchanged (Egaroucid-main/src/engine, (c) Takuto Yamana, GPL-3.0-or-later) and prints
// what the port must reproduce exactly: feature indices, evaluations, hash codes, stable discs, perft counts, the
// Multi-ProbCut error table, book values, and search results with node counts. See README.md for how to build and run it.
//
// Usage: egaroucid-ref <command> [options] < positions
//   eval                 one line per position: hash, mid_evaluate, move-ordering-end evaluation, stable discs, 64 features
//   perft <depth>        leaf counts with and without counting passes as a move
//   search <level> [log] ai(board, level, no book, single thread): move, value, depth, probability, nodes per position;
//                        with "log", the per-iteration log lines (without times) follow each result, prefixed with "#"
//   mpc                  a checksum of the Multi-ProbCut error table and some sample entries (no input)
//   book                 the book value of every legal move that is in the book (loads resources/book.egbk3)
//   random <n> <seed>    generates positions by random play: n for every disc count 4..63, and n where a side passes
//   book-walk <n> <seed> generates n positions by random walks through the book
//
// Positions are read one per line: "[id] <64 squares a1..h8> <side to move>", where X/B/* is black, O/W is white and
// anything else is empty. Lines starting with '#' and empty lines are skipped. Output lines start with the id (or the
// line number when there is none).

#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unistd.h>
#include "engine/engine_all.hpp"

namespace {

std::string binary_directory() {
    char buffer[4096];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) {
        return "./";
    }
    std::string path(buffer, length);
    return path.substr(0, path.find_last_of('/') + 1);
}

void initialize(const std::string &directory, bool with_book) {
    thread_pool.resize(0);
    bit_init();
    mobility_init();
    flip_init();
    last_flip_init();
    endsearch_init();
#if USE_MPC_PRE_CALCULATION
    mpc_init();
#endif
    move_ordering_init();
#if USE_CHANGEABLE_HASH_LEVEL
    hash_resize(DEFAULT_HASH_LEVEL, DEFAULT_HASH_LEVEL, directory, false);
#else
    hash_tt_init(directory, false);
#endif
    stability_init();
    if (!evaluate_init(directory + "resources/eval.egev2", directory + "resources/eval_move_ordering_end.egev", false)) {
        std::cerr << "cannot load the evaluation from " << directory << "resources/" << std::endl;
        std::exit(1);
    }
    if (with_book) {
        book_init(directory + "resources/book.egbk3", false);
    }
}

struct Position {
    std::string id;
    std::string text;
    Board board;
};

bool read_position(std::istream &in, int &line_number, Position &position) {
    std::string line;
    while (std::getline(in, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] == '#') {
            continue;
        }
        // Anything after a ';' is a list of move scores (the benchmark format); it is not needed here.
        std::istringstream words(line.substr(0, line.find(';')));
        std::vector<std::string> parts;
        for (std::string word; words >> word;) {
            parts.push_back(word);
        }
        if (parts.size() == 3 && parts[1].size() == 64) {
            position.id = parts[0];
            parts.erase(parts.begin());
        } else if (parts.size() >= 2 && parts[0].size() == 64) {
            position.id = std::to_string(line_number);
        } else {
            std::cerr << "line " << line_number << " is not a position: " << line << std::endl;
            std::exit(1);
        }
        std::string side = parts[1].substr(0, 1);
        position.text = parts[0] + " " + side;
        if (!position.board.from_str(parts[0] + side)) {
            std::exit(1);
        }
        return true;
    }
    return false;
}

std::string hex(uint64_t value) {
    std::ostringstream text;
    text << std::hex << value;
    return text.str();
}

void command_eval(std::istream &in) {
    int line_number = 0;
    Position position;
    while (read_position(in, line_number, position)) {
        Search search(&position.board, MPC_100_LEVEL, false, false);
        std::cout << position.id << '\t' << position.text << '\t' << position.board.hash() << '\t'
                  << mid_evaluate(&position.board) << '\t' << mid_evaluate_move_ordering_end(&search) << '\t'
                  << hex(calc_stability(position.board.player, position.board.opponent)) << '\t'
                  << hex(calc_stability(position.board.opponent, position.board.player)) << '\t';
        for (int i = 0; i < N_PATTERN_FEATURES; ++i) {
            std::cout << (i ? "," : "") << search.eval.features[0][i];
        }
        std::cout << '\n';
    }
}

void command_perft(std::istream &in, int depth) {
    int line_number = 0;
    Position position;
    while (read_position(in, line_number, position)) {
        Board board = position.board.copy();
        uint64_t with_passes = perft(&board, depth, false);
        uint64_t without_passes = perft_no_pass_count(&board, depth, false);
        std::cout << position.id << '\t' << position.text << '\t' << depth << '\t' << with_passes << '\t'
                  << without_passes << '\n';
    }
}

void command_search(std::istream &in, int level, bool log) {
    int line_number = 0;
    Position position;
    while (read_position(in, line_number, position)) {
        // A fresh start for every position, so a result does not depend on the positions before it.
        transposition_table.init();
        std::memset(lttable, 0, sizeof(lttable));
        std::ostringstream captured;
        std::streambuf *saved = std::cerr.rdbuf();
        if (log) {
            std::cerr.rdbuf(captured.rdbuf());
        }
        Search_result result = ai(position.board, level, false, 0, false, log);
        std::cerr.rdbuf(saved);
        std::cout << position.id << '\t' << position.text << '\t' << level << '\t'
                  << (result.policy >= 0 && result.policy < HW2 ? idx_to_coord(result.policy) : std::string("--")) << '\t'
                  << result.value << '\t' << result.depth << '\t' << result.probability << '\t'
                  << (result.is_end_search ? 1 : 0) << '\t' << result.nodes << '\t' << result.clog_nodes << '\n';
        if (log) {
            std::istringstream lines(captured.str());
            // Times and speeds differ from run to run; they are dropped so the fixtures are reproducible.
            static const std::regex timing(" (time|NPS|nps) [0-9]+");
            for (std::string line; std::getline(lines, line);) {
                std::cout << "# " << std::regex_replace(line, timing, "") << '\n';
            }
        }
    }
}

void command_mpc() {
    // FNV-1a over every entry in index order, as 32-bit little-endian integers.
    uint64_t checksum = 1469598103934665603ULL;
    size_t count = 0;
    for (int level = 0; level < N_SELECTIVITY_LEVEL; ++level) {
        for (int discs = 0; discs <= HW2; ++discs) {
            for (int shallow = 0; shallow < HW2 - 3; ++shallow) {
                for (int deep = 0; deep < HW2 - 3; ++deep) {
                    uint32_t value = (uint32_t)mpc_error[level][discs][shallow][deep];
                    for (int byte = 0; byte < 4; ++byte) {
                        checksum ^= (value >> (8 * byte)) & 0xFF;
                        checksum *= 1099511628211ULL;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << "entries\t" << count << "\nfnv1a\t" << hex(checksum) << '\n';
    const int samples[][4] = {{0, 20, 0, 5}, {1, 30, 2, 8}, {2, 40, 4, 12}, {3, 50, 6, 16}, {4, 24, 3, 9}, {5, 44, 8, 20}, {6, 58, 1, 3}};
    for (const auto &s : samples) {
        std::cout << "mpc_error[" << s[0] << "][" << s[1] << "][" << s[2] << "][" << s[3] << "]\t"
                  << mpc_error[s[0]][s[1]][s[2]][s[3]] << '\n';
    }
}

void command_book(std::istream &in) {
    int line_number = 0;
    Position position;
    while (read_position(in, line_number, position)) {
        Board board = position.board.copy();
        std::cout << position.id << '\t' << position.text << '\t';
        int own;
        std::cout << (book.get_value(board, &own) ? std::to_string(own) : std::string("-")) << '\t';
        // The same child lookup as Book::get_random: the value of each move for the side to move.
        uint64_t legal = board.get_legal();
        Flip flip;
        bool first = true;
        for (uint_fast8_t cell = first_bit(&legal); legal; cell = next_bit(&legal)) {
            calc_flip(&flip, &board, cell);
            board.move_board(&flip);
            int sign = -1;
            if (board.get_legal() == 0ULL) {
                sign = 1;
                board.pass();
            }
            int value;
            if (book.get_value(board, &value)) {
                std::cout << (first ? "" : ",") << idx_to_coord(cell) << ':' << sign * value;
                first = false;
            }
            if (sign == 1) {
                board.pass();
            }
            board.undo_board(&flip);
        }
        std::cout << '\n';
    }
}

// The 64 squares a1..h8 and the side to move, the input format of all commands.
std::string format_position(const Board &board, bool black_to_move) {
    std::string text;
    for (int i = 0; i < HW2; ++i) {
        uint64_t bit = 1ULL << (HW2_M1 - i);
        bool own = board.player & bit, other = board.opponent & bit;
        bool black = black_to_move ? own : other, white = black_to_move ? other : own;
        text += black ? 'X' : white ? 'O' : '-';
    }
    return text + (black_to_move ? " X" : " O");
}

// mt19937 is the same on every platform; distributions are not, so "% n" is used.
int random_below(std::mt19937 &random, int n) {
    return (int)(random() % (uint32_t)n);
}

int random_legal_move(std::mt19937 &random, uint64_t legal) {
    int pick = random_below(random, pop_count_ull(legal));
    for (uint_fast8_t cell = first_bit(&legal);; cell = next_bit(&legal)) {
        if (pick-- == 0) {
            return cell;
        }
    }
}

// Random games from the start position: per_count positions for every disc count 4..63, and up to per_count
// positions where the side to move must pass.
void command_random(int per_count, uint32_t seed) {
    std::mt19937 random(seed);
    for (int discs = 4; discs < HW2; ++discs) {
        int found = 0;
        while (found < per_count) {
            Board board;
            board.reset();
            bool black = true;
            while (board.n_discs() < discs) {
                uint64_t legal = board.get_legal();
                if (legal == 0) {
                    board.pass();
                    black = !black;
                    if (board.get_legal() == 0) {
                        break;
                    }
                    continue;
                }
                Flip flip;
                calc_flip(&flip, &board, random_legal_move(random, legal));
                board.move_board(&flip);
                black = !black;
            }
            if (board.n_discs() == discs && !board.is_end()) {
                std::cout << 'r' << discs << '_' << found << ' ' << format_position(board, black) << '\n';
                ++found;
            }
        }
    }
    int passes = 0;
    while (passes < per_count) {
        Board board;
        board.reset();
        bool black = true;
        while (!board.is_end() && passes < per_count) {
            uint64_t legal = board.get_legal();
            if (legal == 0) {
                std::cout << 'p' << passes++ << ' ' << format_position(board, black) << '\n';
                board.pass();
                black = !black;
                continue;
            }
            Flip flip;
            calc_flip(&flip, &board, random_legal_move(random, legal));
            board.move_board(&flip);
            black = !black;
        }
    }
}

// Walks the book from the start position: each walk plays random book moves (moves whose position is in the book)
// for a random number of plies up to 40 and prints the position where it stops.
void command_book_walk(int count, uint32_t seed) {
    std::mt19937 random(seed);
    for (int i = 0; i < count; ++i) {
        Board board;
        board.reset();
        bool black = true;
        int plies = 1 + random_below(random, 40);
        for (int ply = 0; ply < plies; ++ply) {
            uint64_t legal = board.get_legal();
            std::vector<int> in_book;
            Flip flip;
            for (uint_fast8_t cell = first_bit(&legal); legal; cell = next_bit(&legal)) {
                calc_flip(&flip, &board, cell);
                Board child = board.move_copy(&flip);
                int value;
                if (book.get_value(child, &value)) {
                    in_book.push_back(cell);
                }
            }
            if (in_book.empty()) {
                break;
            }
            calc_flip(&flip, &board, in_book[random_below(random, (int)in_book.size())]);
            board.move_board(&flip);
            black = !black;
            if (board.get_legal() == 0) {
                break;
            }
        }
        std::cout << 'b' << i << ' ' << format_position(board, black) << '\n';
    }
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "usage: egaroucid-ref eval | perft <depth> | search <level> [log] | mpc | book  < positions" << std::endl
                  << "       egaroucid-ref random <n> <seed> | book-walk <n> <seed>" << std::endl;
        return 1;
    }
    std::ios::sync_with_stdio(false);
    std::string command = argv[1];
    initialize(binary_directory(), command == "book" || command == "book-walk");
    if (command == "eval") {
        command_eval(std::cin);
    } else if (command == "perft" && argc >= 3) {
        command_perft(std::cin, std::stoi(argv[2]));
    } else if (command == "search" && argc >= 3) {
        command_search(std::cin, std::stoi(argv[2]), argc >= 4 && std::string(argv[3]) == "log");
    } else if (command == "mpc") {
        command_mpc();
    } else if (command == "book") {
        command_book(std::cin);
    } else if (command == "random" && argc >= 4) {
        command_random(std::stoi(argv[2]), (uint32_t)std::stoul(argv[3]));
    } else if (command == "book-walk" && argc >= 4) {
        command_book_walk(std::stoi(argv[2]), (uint32_t)std::stoul(argv[3]));
    } else {
        std::cerr << "unknown command " << command << std::endl;
        return 1;
    }
    return 0;
}
