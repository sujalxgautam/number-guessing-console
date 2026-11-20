# Number Guessing — Console Edition (C++17)

Number guessing with two modes, four ranges, and turn-based multiplayer.
Pick a range, pick a mode, and go.

A deliberately minimal C++17 project — standard library only, no external
dependencies — built to teach input validation, `<random>` done correctly,
turn-based game logic, and a genuinely instructive bot ladder (random →
no-repeat random → binary search → optimal binary search).

## Features

### Modes

- **Single player** — the computer picks a secret number, you guess until
  you find it. Tracks how many guesses you needed.
- **Multiplayer** — the computer picks a secret number and **two players
  race** to guess it first. Whoever hits it wins the round.

### Multiplayer sub-modes

- **Human vs Human** — you and a friend take turns. Honor system: each
  player looks away during the other's turn.
- **Human vs Bot** — you take turns against a bot. First to find the number
  wins.

### Ranges

`1-10`, `1-100`, `1-500`, `1-1000` — chosen separately from the mode.

### Bot difficulty (Human vs Bot only)

| Level | Strategy |
|-------|----------|
| Easy | Random guesses, may repeat |
| Mid | Random guesses, never repeats |
| Hard | Binary search |
| Extreme | Optimal binary search — near-impossible to beat fairly |

The bot ladder is the interesting part: **Easy is a coin flip, Extreme is
mathematically optimal**, and the middle two exist to show *why* binary
search beats random. A player who can't beat Extreme hasn't made a mistake —
Extreme is doing the optimal thing, which is the lesson.

## Controls

| Key | Action |
|-----|--------|
| `1`–`4` | Pick a menu option |
| `R` / `M` / `Q` | Play again / Main menu / Quit (post-round) |
| `Q` | Quit (from a guess prompt) |

## Build & Run

### Quick build (g++)

```bash
g++ -std=c++17 -O2 main.cpp -o guess
./guess
```

*(No `-pthread` — this project has no threading.)*

### With CMake

```bash
cmake -B build
cmake --build build
./guess
```

## How it works

- **One `std::mt19937` seeded once** at startup, then reused for every
  round. Seeding inside the loop would produce the same number every game —
  a classic mistake the header warns about.
- **Input is validated in a loop.** `readIntInRange` parses, checks
  numeric-ness and range, re-prompts on failure, and accepts `Q` to quit.
  Bad input never reaches the game logic.
- **Human vs Human uses the honor system.** Players look away during each
  other's turn. This is a deliberate design choice for a terminal game —
  the alternative (hiding the screen between turns) is a polish item.
- **The bot is a small state machine.** Each level is a different strategy
  object; `Human vs Bot` just calls `g.nextGuess(lastGuess, lastResult)`
  and lets the strategy decide. The Extreme bot narrows its bracket by
  exactly the information from each hint, which is what makes it optimal.

## What this project teaches

- `<random>` done correctly: seed once, use distributions, never `rand() % n`.
- Input validation that loops until valid — bad input is impossible to reach
  the game.
- Turn-based state machines (no real-time loop needed here).
- **A bot ladder** — random → no-repeat → binary search → optimal — that
  makes the value of binary search *visible* by playing against it.
- Why `namespace round` is a bug (collides with `std::round`) — the header
  documents this real pitfall.

## Requirements

- **Compiler:** any C++17 compiler (`g++`, `clang++`, MSVC)
- **Platform:** Linux, macOS, or Windows
- **Terminal:** ANSI color + UTF-8 support (default in modern terminals)
- **Dependencies:** standard library only
- **Threads:** none required

## Roadmap

- [ ] Persist best (fewest guesses) per range to disk
- [ ] Screen-hide mode for Human vs Human (instead of honor system)
- [ ] "Hot/cold" proximity hints (closer / farther) instead of only high/low
- [ ] Bot "Hard" difficulty ramp (mix random and binary search)
- [ ] Multi-round scoring in multiplayer (first to 3 wins)

## Notes

- The Extreme bot is the project's teaching centerpiece: it plays optimally,
  so losing to it isn't a bug — it's the lesson.
- The binary is named `guess`. Both the `g++` command and the CMake build
  produce it at the project root.

## License

MIT.