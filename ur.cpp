// Play the Royal Game of Ur.
//
// Data Model
// ----------
// The game state consists of both of the players' sides of the board.
//
// The positions in which a tile can be placed are numbered [0..15] along the
// tile path. That is, position 0 represents the starting pile and position 15
// represents the ending pile. This also means one player's position 4 is a
// different cell than the other player's position 4, but they share positions 5
// through 12.
//
// Given a fixed number of steps, a move can be characterized by its starting
// position. So, we represent a set of valid moves by a size-16 bitset, one for
// each position (although position 15 is never a valid position to begin a
// move). For example, if a collection of options for a fixed number of steps
// has bit 5 set, then it is a valid move for the current player to move their
// tile starting from position 5.
//
// Each "side" requires at most 17 bits (3 bits for the [0..7] pile and 14 for
// the occupation of the tile path). However, for convenience, we store it as:
// - The number of tiles in the starting pile, as a uint16_t; and
// - The positions in which a tile is present, as a size-16 bitset.
//   - As before, bit 15 is unused. Furthermore, bit 0 is redundant with the
//     number of remaining tiles in the starting pile.
//
// Lastly... if you think this is hard to read - I had to write it. :)
#include <bitset>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>


// We'd use smaller types if we could. However, be aware that some operations
// require larger-width types, so we occasionally widen and narrow freely.
using Steps = uint8_t;  // [0..4]
using Position = uint8_t;  // [0..15]

using Options = std::bitset<16>;  // Bit 15 is unused.

using Side = struct _Side {
    // The number of tiles remaining in the starting pile.
    uint16_t remaining;
    // The positional occupancy of the tile path.
    // Bit 15 is unused.
    // Bit 0 is redundant with `remaining`.
    std::bitset<16> occupied;
};
inline bool operator==(const _Side& lhs, const _Side& rhs) {
    return lhs.remaining == rhs.remaining && lhs.occupied == rhs.occupied;
}
inline bool operator!=(const _Side& lhs, const _Side& rhs) {
    return !(lhs == rhs);
}

// The number of starting tiles per player.
constexpr uint16_t TILES = 7;

// You start with a pile of tiles, so only position 0 is occupied.
constexpr Side START{TILES, 1};
// You end with no tiles remaining and no straggling tiles still on the path.
constexpr Side COMPLETE{0, 0};

// Enable/disable more comprehensive game logging.
constexpr bool VERBOSE = true;


// Roll the tetrahedra by sampling from Bin(4, 0.5).
//
// Unfamiliar with (pseudo-)randomness in C++?
// See: https://en.cppreference.com/w/cpp/numeric/random
[[ nodiscard ]] Steps getRandomRoll() {
    // NOTE(sredmond): These are static locals only because of laziness.
    // Randomly seed the (32-bit) generator.
    static std::random_device rd;
    static std::mt19937 gen(rd());

    static std::binomial_distribution<Steps> d(4, 0.5);
    return d(gen);
}


// Get the valid options for the next move.
//
// Bit `i` of the returned bitset is set iff a move at position `i` is valid::
//
//     const Options options = getOptions(...);
//     if (options[4]) ...  // You can move from position 4.
//
[[ nodiscard ]] const Options getOptions(Side self, Side other, Steps steps) {
    // Every position starts as valid.
    Options options{0x7FFF};
    // A position is valid iff all of the following are true:
    // 1. You have a piece at that position.
    options &= self.occupied;
    // 2. You don't land on your own piece.
    options &= ~(self.occupied & self.occupied >> steps);
    // 3. You don't land on the central rosette (bit 8) when already occupied.
    options &= ~(std::bitset<16>{0x0100} & other.occupied);
    // 4. You don't land too far off the board.
    options &= 0xFFFF >> steps;
    return options;
}


// Attempt to apply a move, and return whether the current player goes again.
//
// The game state (i.e. the two sides) are updated by reference.
//
// Pre: The proposed move is valid. This isn't the place for error checking.
[[ nodiscard ]] bool apply(Side& self, Side& other, Position start, Steps steps) {
    Position end = start + steps;

    // Pick up the piece from the start of the move...
    if (start == 0) {
        self.remaining--;
        if (self.remaining == 0) self.occupied.reset(0);
    }
    else self.occupied.reset(start);
    // ...and place it at the end of the move.
    if (end < 15) self.occupied.set(end);


    // Reset the opponent's piece if we land on them in the middle.
    // It's already prohibited to land on them on the central rosette.
    if (5 <= end && end <= 12 && other.occupied.test(end)) {
        other.occupied.reset(end);
        other.remaining++;
        other.occupied.set(0);
    }

    // Go again if we ended on a rosette.
    return end == 4 || end == 8 || end == 14;
}


// "Visualize" the game board.
//
//     ....  ..
//     ........
//     ....  ..
//
// The pieces of the top player are labelled 'T' and those of the bottom 'B'.
// The blank spaces are filled with the tiles remaining and score per player.
//
// An in-progress game might look like::
//
//     .TT.31..
//     ...T..B.
//     B...50..
//
// Pre: The game state is valid.
void display(Side top, Side bottom) {
    // Have a better idea? I'm open to other implementations. :)
    char content[] = "....00..\n........\n....00..";

    // The path of a game piece through our string representation.
    Position topPath[16] = {4,3,2,1,0,9,10,11,12,13,14,15,16,7,6,5};
    Position bottomPath[16] = {22,21,20,19,18,9,10,11,12,13,14,15,16,25,24,23};

    // Increment the counters (as character arithmetic, e.g. '9' + 1 == ':').
    content[topPath[0]] += top.remaining;
    content[bottomPath[0]] += bottom.remaining;
    // Don't double-count position 0.
    content[topPath[15]] += TILES - top.remaining - (top.occupied >> 1).count();
    content[bottomPath[15]] += TILES - bottom.remaining - (bottom.occupied >> 1).count();

    // Mark tiles actually on the game board.
    for (size_t i = 1; i < 15; ++i) {
        if (top.occupied.test(i)) content[topPath[i]] = 'T';
        if (bottom.occupied.test(i)) content[bottomPath[i]] = 'B';
    }

    // Print the game state.
    std::cout << content << std::endl;
}


// Verify that the game state is valid.
bool _verifySides(Side self, Side other) {
    // The game state is valid iff there are no collisions in the middle.
    return (self.occupied & other.occupied & std::bitset<16>{0x1FE0}) == 0;
}


/**********
 * AGENTS *
 **********/

// An abstract agent for Ur.
//
// A concrete subclass must override `getMove(...)`. If an implementation wants
// to signal a failure, it should return `INVALID`.
//
// An agent should be constructed with a name, although subclasses can choose to
// provide a default name.
class Agent {
public:
    Agent(std::string name) : _name(name) { /* empty */ }
    virtual ~Agent() { /* empty */ };
    virtual Position getMove(Side self, Side other, Steps steps, Options options) = 0;
    [[ nodiscard ]] std::string getName() const { return _name; }

    static constexpr Position INVALID{15};  // It's invalid to move from spot 15.
protected:
    std::string _name;
};


// A concrete agent that advances the piece farthest from the end.
class FarthestAgent : public Agent {
public:
    FarthestAgent() : Agent("Furthest") { /* empty */ }
    virtual Position getMove(Side self, Side other, Steps steps, Options options) {
        for (size_t i = 0; i < 15; ++i) {
            if (options.test(i)) return i;
        }
        return INVALID;
    }
};


// A concrete agent that advances the piece closest to the end.
class ClosestAgent : public Agent {
public:
    ClosestAgent() : Agent("Closest") { /* empty */ }
    virtual Position getMove(Side self, Side other, Steps steps, Options options) {
        for (int i = 14; i >= 0; --i) {  // Needs to be int, not size_t.
            if (options.test(i)) return i;
        }
        return INVALID;
    }
};


// A concrete agent that asks the user to choose from among available options.
class InteractiveAgent : public Agent {
public:
    InteractiveAgent(std::string name) : Agent(name) { /* empty */}
    virtual Position getMove(Side self, Side other, Steps steps, Options options) {
        std::cout << "Hello, " << _name << "!" << std::endl;
        std::cout << "The current state (you are shown on top) is: " << std::endl;
        display(self, other);
        std::cout << "You rolled a " << +steps << "." << std::endl;
        std::cout << "Your options are: " << std::endl;
        for (size_t i = 0; i < 15; ++i) {
            if (options.test(i)) std::cout << "> " << i << std::endl;
        }
        // Read the user's input.
        std::cout << "What do you choose? ";
        std::cout.flush();
        uint16_t move = INVALID;  // Be wary of implicit widening and narrowing.
        while (true) {
            std::string line;
            if (!getline(std::cin, line)) {
                std::cerr << "Unexpected end of input." << std::endl;
            }
            // Ideally, I'd also trim the line.
            std::istringstream stream(line);
            stream >> move;
            if (stream.fail() || !stream.eof()) {
                std::cerr << "Illegal format." << std::endl;
                std::cout << "Please try again: ";
                std::cout.flush();
                continue;
            }
            if (!options[move]) {
                std::cerr << "Invalid option." << std::endl;
                std::cout << "Please try again: ";
                std::cout.flush();
                continue;
            }
            break;
        }
        return move;
    }
};


/************
 * GAMEPLAY *
 ************/

// Play out one roll and return whether the current player goes again.
bool playOneRoll(const std::unique_ptr<Agent>& player, Side& self, Side& other) {
    std::string name = player->getName();

    // Roll the tetrahedra to determine the number of steps.
    Steps steps = getRandomRoll();

    if (VERBOSE) std::cout << name << " rolls a " << +steps << "." << std::endl;

    // Don't bother asking the agent for a move if the roll was a zero.
    if (steps == 0) return false;

    // Precompute the valid moves. Sometimes there are none, so we move on.
    Options options = getOptions(self, other, steps);
    if (options == 0) {
        if (VERBOSE) std::cout << "No legal moves." << std::endl;
        return false;
    }

    // Ask the agent for a move.
    Position start = player->getMove(self, other, steps, options);
    if (VERBOSE) std::cout << name << " chooses " << +start << "." << std::endl;

    // Submitting an invalid move passes your turn.
    if (start == Agent::INVALID || !options[start]) {
        if (VERBOSE) std::cout << "Oh no! An invalid move..." << std::endl;
        return false;
    }

    // Apply the move to the game state.
    return apply(self, other, start, steps);
}


// Play one game of Ur.
bool playOneGame(const std::unique_ptr<Agent>& first, const std::unique_ptr<Agent>& second) {
    Side left = START;
    Side right = START;

    uint64_t rolls = 0;  // Track the length of the game.
    bool current = true;  // Whether the current player is the first player.
    while (left != COMPLETE && right != COMPLETE) {
        if (VERBOSE) display(left, right);

        const std::unique_ptr<Agent>& player = current ? first : second;

        // The current player's side is `self`; the opponent's side is `other`.
        Side& self = current ? left : right;
        Side& other = current ? right : left;

        // Let the current player play out a roll.
        bool again = playOneRoll(player, self, other);
        ++rolls;
        current = !(current ^ again);
    }
    if (VERBOSE) std::cout << "Ended after " << rolls << " rolls." << std::endl;
    return left == COMPLETE;
}


// Play the Royal Game of Ur, repeatedly.
int main(int argc, char *argv[]) {
    std::cout << "Hello, world! Welcome to the Royal Game of Ur." << std::endl;

    // Construct some Ur-playing agents.
    std::unique_ptr<Agent> sam = std::make_unique<InteractiveAgent>("Sam");
    std::unique_ptr<Agent> farthest = std::make_unique<FarthestAgent>();
    std::unique_ptr<Agent> closest = std::make_unique<ClosestAgent>();

    // Play one game against the AI.
    playOneGame(sam, closest);

    // Simulate many games between the AIs.
    size_t repeats = 10000;
    size_t firstPlayerWins = 0;
    for (size_t i = 0; i < repeats; ++i) {
        if (playOneGame(farthest, closest)) {
            firstPlayerWins++;
        }
    }
    std::cout << "First player won " << firstPlayerWins << " / " << repeats << std::endl;

    return EXIT_SUCCESS;
}
