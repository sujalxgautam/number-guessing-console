// =============================================================================
//  main.cpp — Number Guessing Game (Console Edition) — v3
// =============================================================================
//
//  PROJECT BRIEF
//  -------------
//  Number guessing with two modes, four ranges, and turn-based multiplayer.
//
//  MODES
//  -----
//    SINGLE PLAYER
//      Computer picks a secret number. You guess until you find it.
//
//    MULTIPLAYER
//      Computer picks a secret number. Two players take turns guessing.
//      Whoever guesses it first WINS THE ROUND. The number is unknown to
//      both players until someone hits it.
//
//      Two sub-modes:
//        a) Human vs Human — you and a friend. Turn-based. First to win.
//        b) Human vs Bot   — you and the bot. Turn-based. First to win.
//
//  RANGES (chosen separately from mode):
//    1-10, 1-100, 1-500, 1-1000
//
//  BOT DIFFICULTY (Multiplayer -> Human vs Bot only):
//    Easy    — random guesses, may repeat.
//    Mid     — random guesses, never repeats.
//    Hard    — binary search.
//    Extreme — optimal binary search (near-impossible to beat).
//
//  DIFFICULTY : Easy
//  STANDARD    : C++17
//  DEPENDENCIES: Standard library only. No -pthread needed.
//  BUILD       : g++ -std=c++17 -O2 main.cpp -o guess
//  RUN         : ./guess
//
//  NAMING PITFALL
//  --------------
//  `round` is a function in <cmath>. Never name a namespace `round`.
//  We use `gameRound`.
//
//  COMMON PITFALLS
//  ---------------
//    * rand() % n -> biased. Use <random>.
//    * Seeding inside the loop -> same number every game. Seed ONCE.
//    * cin >> int on bad input -> stream stuck. clear+ignore.
//    * Checking bounds BEFORE equality -> off-by-one.
//    * Naming a namespace `round` -> collides with std::round.
//    * Turn-based races: DON'T let both players guess from same screen.
//    * Using render::clearScreen() at startup -> only moves cursor home,
//      does NOT wipe prior content. Use color::clearScreen().
//
//  LICENSE: MIT.
// =============================================================================

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
//  ANSI color helpers
// =============================================================================
namespace color {
    constexpr const char* kReset    = "\033[0m";
    constexpr const char* kBold     = "\033[1m";
    constexpr const char* kDim      = "\033[2m";
    constexpr const char* kRed      = "\033[31m";
    constexpr const char* kGreen    = "\033[32m";
    constexpr const char* kYellow   = "\033[33m";
    constexpr const char* kCyan     = "\033[36m";
    constexpr const char* kMagenta  = "\033[35m";
    constexpr const char* kBoldRed  = "\033[1;31m";
    constexpr const char* kBoldCyan = "\033[1;36m";
    constexpr const char* kBoldGreen= "\033[1;32m";
    constexpr const char* kBoldYellow="\033[1;33m";
    constexpr const char* kBoldMagenta="\033[1;35m";

    inline void clearScreen() { std::cout << "\033[H\033[2J"; }
}

// =============================================================================
//  Types
// =============================================================================
enum class Mode      { Single, Multiplayer };
enum class SubMode   { HumanVsHuman, HumanVsBot };
enum class BotLevel  { Easy, Mid, Hard, Extreme };

struct RangeSpec {
    const char* label;
    int         lo;
    int         hi;
};

constexpr RangeSpec kRanges[] = {
    { "1-10",   1,   10  },
    { "1-100",  1,   100 },
    { "1-500",  1,   500 },
    { "1-1000", 1,   1000 },
};

inline const char* botName(BotLevel b) {
    switch (b) {
        case BotLevel::Easy:    return "Easy";
        case BotLevel::Mid:     return "Mid";
        case BotLevel::Hard:    return "Hard";
        case BotLevel::Extreme: return "Extreme";
    }
    return "?";
}

// =============================================================================
//  Input helpers
// =============================================================================
namespace input {

inline void trim(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
}

inline std::optional<std::string> readLine() {
    std::string line;
    if (!std::getline(std::cin, line)) return std::nullopt;
    return line;
}

inline std::optional<char> readChoice() {
    auto line = readLine();
    if (!line) return std::nullopt;
    trim(*line);
    if (line->empty()) return '\0';
    return static_cast<char>(std::toupper((*line)[0]));
}

inline std::optional<int> readIntInRange(int lo, int hi) {
    while (true) {
        std::cout << "  " << color::kCyan << "> " << color::kReset << std::flush;
        auto line = readLine();
        if (!line) return std::nullopt;
        trim(*line);
        if (line->empty()) continue;
        if (*line == "q" || *line == "Q") return std::nullopt;

        try {
            std::size_t pos = 0;
            const int n = std::stoi(*line, &pos);
            if (pos != line->size()) { std::cout << "  Not a number.\n"; continue; }
            if (n < lo || n > hi) {
                std::cout << "  Must be between " << lo << " and " << hi << ".\n";
                continue;
            }
            return n;
        } catch (...) {
            std::cout << "  Not a number.\n";
        }
    }
}

} // namespace input

// =============================================================================
//  Bots
// =============================================================================
namespace bot {

struct Guesser {
    BotLevel level;
    int      lo, hi;
    std::mt19937 rng;

    std::set<int> alreadyGuessed;
    int bsLo, bsHi;

    Guesser(BotLevel lvl, int rangeLo, int rangeHi)
        : level(lvl), lo(rangeLo), hi(rangeHi),
          rng(std::random_device{}()),
          bsLo(rangeLo), bsHi(rangeHi) {}

    int nextGuess(int lastGuess, int lastResult) {
        switch (level) {
            case BotLevel::Easy:    return easyGuess();
            case BotLevel::Mid:     return midGuess();
            case BotLevel::Hard:    return hardGuess(lastGuess, lastResult);
            case BotLevel::Extreme: return extremeGuess(lastGuess, lastResult);
        }
        return lo;
    }

private:
    int easyGuess() {
        std::uniform_int_distribution<int> d(lo, hi);
        return d(rng);
    }

    int midGuess() {
        std::vector<int> candidates;
        candidates.reserve(static_cast<std::size_t>(hi - lo + 1));
        for (int i = lo; i <= hi; ++i)
            if (!alreadyGuessed.count(i)) candidates.push_back(i);
        if (candidates.empty()) return lo;
        std::uniform_int_distribution<std::size_t> d(0, candidates.size() - 1);
        int g = candidates[d(rng)];
        alreadyGuessed.insert(g);
        return g;
    }

    int hardGuess(int lastGuess, int lastResult) {
        if (lastResult == 0) return (bsLo + bsHi) / 2;
        if (lastResult < 0)  bsLo = lastGuess + 1;
        else                 bsHi = lastGuess - 1;
        return (bsLo + bsHi) / 2;
    }

    int extremeGuess(int lastGuess, int lastResult) {
        if (lastResult == 0) return (bsLo + bsHi) / 2;
        if (lastResult < 0) bsLo = std::max(bsLo, lastGuess + 1);
        else                bsHi = std::min(bsHi, lastGuess - 1);
        return (bsLo + bsHi) / 2;
    }
};

inline int optimalGuesses(int lo, int hi) {
    int n = hi - lo + 1;
    int guesses = 0;
    while (n > 1) { n = (n + 1) / 2; ++guesses; }
    return guesses;
}

} // namespace bot

// =============================================================================
//  Game round logic
// =============================================================================
namespace gameRound {

using namespace color;

inline std::optional<int> humanGuesses(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    const int target = dist(rng);

    std::cout << "  " << kDim
              << "The computer picked a number in [" << lo << ", " << hi << "]."
              << kReset << "\n\n";

    int attempts = 0;
    while (true) {
        std::cout << "  " << kCyan << "Guess (" << lo << "-" << hi
                  << ", Q to quit): " << kReset << std::flush;
        auto guess = input::readIntInRange(lo, hi);
        if (!guess) return std::nullopt;
        ++attempts;

        if (*guess == target) {
            std::cout << "  " << kBoldGreen << "Correct!" << kReset
                      << "  Found in " << kBold << attempts << kReset
                      << " guess" << (attempts == 1 ? "" : "es") << ".\n";
            return attempts;
        }
        if (*guess > target) std::cout << "  " << kRed    << "Too high." << kReset << "\n";
        else                 std::cout << "  " << kYellow << "Too low."  << kReset << "\n";
    }
}

inline std::optional<int> humanVsHuman(std::mt19937& rng, int lo, int hi,
                                       int& winnerOut, int& p1GuessCount, int& p2GuessCount) {
    std::uniform_int_distribution<int> dist(lo, hi);
    const int target = dist(rng);

    color::clearScreen();
    std::cout << "  " << kDim
              << "The computer picked a secret number in [" << lo << ", " << hi << "]."
              << kReset << "\n"
              << "  " << kDim
              << "Take turns guessing. First to find it wins the round."
              << kReset << "\n\n";

    int  currentPlayer = 1;
    int  lastGuess     = 0;
    int  lastResult    = 0;
    int  attempts      = 0;

    while (true) {
        const char* playerColor = (currentPlayer == 1) ? kBoldCyan : kBoldMagenta;
        const char* playerName  = (currentPlayer == 1) ? "Player 1" : "Player 2";

        std::cout << "  " << playerColor << playerName << kReset
                  << ", it's your turn.\n";
        std::cout << "  " << kDim
                  << "(Other player, look away — this uses the honor system.)"
                  << kReset << "\n\n";

        auto guess = input::readIntInRange(lo, hi);
        if (!guess) return std::nullopt;
        ++attempts;
        lastGuess  = *guess;
        lastResult = (*guess < target) ? -1 : (*guess > target ? +1 : 0);

        if (currentPlayer == 1) p1GuessCount++; else p2GuessCount++;

        if (*guess == target) {
            winnerOut = currentPlayer;
            color::clearScreen();
            std::cout << "  " << kBoldGreen
                      << playerName << " wins! " << kReset
                      << " Guessed it in " << attempts << " total guesses.\n"
                      << "  " << kDim
                      << "The number was " << target << "."
                      << kReset << "\n";
            return attempts;
        }

        std::cout << "  " << (*guess > target ? kRed : kYellow)
                  << (*guess > target ? "Too high." : "Too low.")
                  << kReset << "\n";

        std::cout << "\n  " << kDim
                  << "Press Enter to pass to the next player..."
                  << kReset << std::flush;
        std::string _;
        std::getline(std::cin, _);

        color::clearScreen();
        currentPlayer = (currentPlayer == 1) ? 2 : 1;
    }
}

inline std::optional<int> humanVsBot(std::mt19937& rng, BotLevel lvl,
                                     int lo, int hi,
                                     int& winnerOut, int& humanGuessCount, int& botGuessCount) {
    std::uniform_int_distribution<int> dist(lo, hi);
    const int target = dist(rng);

    color::clearScreen();
    std::cout << "  " << kDim
              << "The computer picked a secret number in [" << lo << ", " << hi << "]."
              << kReset << "\n"
              << "  " << kDim
              << "You and the bot (" << botName(lvl) << ") take turns guessing."
              << kReset << "\n"
              << "  " << kDim
              << "First to find it wins the round."
              << kReset << "\n\n";

    bot::Guesser g(lvl, lo, hi);
    int humanTurn   = 1;
    int lastGuess   = 0;
    int lastResult  = 0;
    int attempts    = 0;

    while (true) {
        if (humanTurn) {
            std::cout << "  " << kBoldCyan << "Your turn" << kReset << "\n";
            auto guess = input::readIntInRange(lo, hi);
            if (!guess) return std::nullopt;
            ++attempts;
            humanGuessCount++;

            if (*guess == target) {
                winnerOut = 1;
                color::clearScreen();
                std::cout << "  " << kBoldGreen << "You win!" << kReset
                          << "  In " << attempts << " total guesses.\n"
                          << "  " << kDim << "The number was " << target
                          << "." << kReset << "\n";
                return attempts;
            }

            std::cout << "  " << (*guess > target ? kRed : kYellow)
                      << (*guess > target ? "Too high." : "Too low.")
                      << kReset << "\n\n";
        } else {
            const int guess = g.nextGuess(lastGuess, lastResult);
            ++attempts;
            botGuessCount++;
            lastGuess = guess;
            lastResult = (guess < target) ? -1 : (guess > target ? +1 : 0);

            std::cout << "  " << kBoldMagenta << "Bot" << kReset
                      << " (" << botName(lvl) << ") guesses: "
                      << kBold << guess << kReset << "\n";

            if (guess == target) {
                winnerOut = 2;
                color::clearScreen();
                std::cout << "  " << kBoldRed << "Bot wins." << kReset
                          << "  In " << attempts << " total guesses.\n"
                          << "  " << kDim << "The number was " << target
                          << "." << kReset << "\n";
                return attempts;
            }

            std::cout << "  " << (guess > target ? kRed : kYellow)
                      << (guess > target ? "Too high." : "Too low.")
                      << kReset << "\n\n";
        }

        humanTurn = !humanTurn;
    }
}

} // namespace gameRound

// =============================================================================
//  Rendering
// =============================================================================
namespace render {
    
inline void header(const std::string& subtitle = "") {
    std::cout << "\n"
              << color::kBoldCyan
              << "    ███╗   ██╗██╗   ██╗███╗   ███╗██████╗ ███████╗██████╗ \n"
              << "    ████╗  ██║██║   ██║████╗ ████║██╔══██╗██╔════╝██╔══██╗\n"
              << "    ██╔██╗ ██║██║   ██║██╔████╔██║██████╔╝█████╗  ██████╔╝\n"
              << "    ██║╚██╗██║██║   ██║██║╚██╔╝██║██╔══██╗██╔══╝  ██╔══██╗\n"
              << "    ██║ ╚████║╚██████╔╝██║ ╚═╝ ██║██████╔╝███████╗██║  ██║\n"
              << "    ╚═╝  ╚═══╝ ╚═════╝ ╚═╝     ╚═╝╚═════╝ ╚══════╝╚═╝  ╚═╝\n"
              << "              ██████╗ ██╗   ██╗███████╗███████╗███████╗\n"
              << "             ██╔════╝ ██║   ██║██╔════╝██╔════╝██╔════╝\n"
              << "             ██║  ███╗██║   ██║█████╗  ███████╗███████╗\n"
              << "             ██║   ██║██║   ██║██╔══╝  ╚════██║╚════██║\n"
              << "             ╚██████╔╝╚██████╔╝███████╗███████║███████║\n"
              << "              ╚═════╝  ╚═════╝ ╚══════╝╚══════╝╚══════╝\n"
              << color::kReset << "\n"
              << color::kDim << "                     · · ·  console edition  · · ·\n"
              << color::kReset << "\n";
    if (!subtitle.empty())
        std::cout << "  " << color::kDim << subtitle << color::kReset << "\n\n";
}

} // namespace render

// =============================================================================
//  Menus
// =============================================================================
namespace menu {

inline std::optional<int> rangeMenu() {
    while (true) {
        color::clearScreen();
        render::header("Choose a range");
        std::cout << "   " << color::kBold << "1" << color::kReset << "  1-10\n"
                  << "   " << color::kBold << "2" << color::kReset << "  1-100\n"
                  << "   " << color::kBold << "3" << color::kReset << "  1-500\n"
                  << "   " << color::kBold << "4" << color::kReset << "  1-1000\n"
                  << "   " << color::kBold << "Q" << color::kReset << "  Quit\n\n"
                  << "  " << color::kCyan << "Choice: " << color::kReset << std::flush;
        auto c = input::readChoice();
        if (!c) return std::nullopt;
        if (*c == '\0') continue;
        if (*c == '1') return 0;
        if (*c == '2') return 1;
        if (*c == '3') return 2;
        if (*c == '4') return 3;
        if (*c == 'Q') return std::nullopt;
    }
}

inline std::optional<Mode> modeMenu() {
    while (true) {
        color::clearScreen();
        render::header("Choose a mode");
        std::cout << "   " << color::kBold << "1" << color::kReset << "  Single player  "
                  << color::kReset << "\n"
                  << "   " << color::kBold << "2" << color::kReset << "  Multiplayer    "
                  << color::kDim << "(race to guess the number first)" << color::kReset << "\n"
                  << "   " << color::kBold << "Q" << color::kReset << "  Quit\n\n"
                  << "  " << color::kCyan << "Choice: " << color::kReset << std::flush;
        auto c = input::readChoice();
        if (!c) return std::nullopt;
        if (*c == '\0') continue;
        if (*c == '1') return Mode::Single;
        if (*c == '2') return Mode::Multiplayer;
        if (*c == 'Q') return std::nullopt;
    }
}

inline std::optional<SubMode> subModeMenu() {
    while (true) {
        color::clearScreen();
        render::header("Multiplayer mode");
        std::cout << "   " << color::kBold << "1" << color::kReset << "  Human vs Human "
                  << color::kDim << "(two players race to guess)" << color::kReset << "\n"
                  << "   " << color::kBold << "2" << color::kReset << "  Human vs Bot   "
                  << color::kDim << "(you race the bot)" << color::kReset << "\n"
                  << "   " << color::kBold << "B" << color::kReset << "  Back\n\n"
                  << "  " << color::kCyan << "Choice: " << color::kReset << std::flush;
        auto c = input::readChoice();
        if (!c) return std::nullopt;
        if (*c == '\0') continue;
        if (*c == '1') return SubMode::HumanVsHuman;
        if (*c == '2') return SubMode::HumanVsBot;
        if (*c == 'B') return std::nullopt;
    }
}

inline std::optional<BotLevel> botMenu() {
    while (true) {
        color::clearScreen();
        render::header("Bot difficulty");
        std::cout << "   " << color::kBold << "1" << color::kReset << "  Easy     "
                  << color::kDim << "(random, may repeat — easy to beat)"
                  << color::kReset << "\n"
                  << "   " << color::kBold << "2" << color::kReset << "  Mid      "
                  << color::kDim << "(random, no repeats — a fair race)"
                  << color::kReset << "\n"
                  << "   " << color::kBold << "3" << color::kReset << "  Hard     "
                  << color::kDim << "(binary search — you'll have to be fast)"
                  << color::kReset << "\n"
                  << "   " << color::kBold << "4" << color::kReset << "  Extreme  "
                  << color::kBoldYellow << "(optimal binary — almost impossible to beat)"
                  << color::kReset << "\n"
                  << "   " << color::kBold << "B" << color::kReset << "  Back\n\n"
                  << "  " << color::kCyan << "Choice: " << color::kReset << std::flush;
        auto c = input::readChoice();
        if (!c) return std::nullopt;
        if (*c == '\0') continue;
        if (*c == '1') return BotLevel::Easy;
        if (*c == '2') return BotLevel::Mid;
        if (*c == '3') return BotLevel::Hard;
        if (*c == '4') return BotLevel::Extreme;
        if (*c == 'B') return std::nullopt;
    }
}

enum class PostRound { PlayAgain, MainMenu, Quit };

inline PostRound postRoundMenu() {
    while (true) {
        std::cout << "\n  " << color::kBold << "[R]" << color::kReset << " Play again   "
                  << color::kBold << "[M]" << color::kReset << " Main menu   "
                  << color::kBold << "[Q]" << color::kReset << " Quit\n  "
                  << color::kCyan << "Choice: " << color::kReset << std::flush;
        auto c = input::readChoice();
        if (!c) return PostRound::Quit;
        if (*c == '\0') continue;
        if (*c == 'R') return PostRound::PlayAgain;
        if (*c == 'M') return PostRound::MainMenu;
        if (*c == 'Q') return PostRound::Quit;
    }
}

} // namespace menu

// =============================================================================
//  main
// =============================================================================
int main() {
    using namespace color;

    // Full terminal clear at startup — matches Tic-Tac-Toe and the others.
    // NOTE: render::clearScreen() only moves the cursor home; it does NOT
    // wipe prior terminal content. color::clearScreen() does a full clear.
    // Use the full clear here, once, at startup.
    color::clearScreen();

    std::mt19937 rng{std::random_device{}()};

    while (true) {
        auto rangeIdx = menu::rangeMenu();
        if (!rangeIdx) break;
        const auto& range = kRanges[*rangeIdx];

        auto mode = menu::modeMenu();
        if (!mode) break;

        std::optional<SubMode>  subMode;
        std::optional<BotLevel> botLevel;
        if (*mode == Mode::Multiplayer) {
            subMode = menu::subModeMenu();
            if (!subMode) continue;
            if (*subMode == SubMode::HumanVsBot) {
                botLevel = menu::botMenu();
                if (!botLevel) continue;
            }
        }

        bool keepPlaying = true;
        while (keepPlaying) {
            color::clearScreen();

            std::string sub;
            if (*mode == Mode::Single) {
                sub = std::string("Range: ") + range.label + "  ·  Single player";
            } else if (*subMode == SubMode::HumanVsHuman) {
                sub = std::string("Range: ") + range.label + "  ·  Human vs Human";
            } else {
                sub = std::string("Range: ") + range.label
                    + "  ·  Human vs Bot (" + botName(*botLevel) + ")";
            }
            render::header(sub);

            if (*mode == Mode::Single) {
                auto attempts = gameRound::humanGuesses(rng, range.lo, range.hi);
                if (!attempts) return 0;
            } else if (*subMode == SubMode::HumanVsHuman) {
                int winner = 0, p1 = 0, p2 = 0;
                auto attempts = gameRound::humanVsHuman(rng, range.lo, range.hi,
                                                        winner, p1, p2);
                if (!attempts) return 0;
                std::cout << "\n  " << kDim
                          << "Guesses taken — Player 1: " << p1
                          << ", Player 2: " << p2
                          << kReset << "\n";
            } else {
                int winner = 0, human = 0, bot = 0;
                auto attempts = gameRound::humanVsBot(rng, *botLevel,
                                                      range.lo, range.hi,
                                                      winner, human, bot);
                if (!attempts) return 0;
                std::cout << "\n  " << kDim
                          << "Guesses taken — You: " << human
                          << ", Bot: " << bot
                          << kReset << "\n";
            }

            switch (menu::postRoundMenu()) {
                case menu::PostRound::PlayAgain: keepPlaying = true;  break;
                case menu::PostRound::MainMenu:  keepPlaying = false; break;
                case menu::PostRound::Quit:      return 0;
            }
        }
    }

    color::clearScreen();
    std::cout << "\n  " << kBoldGreen << "Thanks for playing!" << kReset << "\n\n";
    return 0;
}