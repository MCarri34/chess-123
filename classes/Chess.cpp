#include "Chess.h"
#include <limits>
#include <cmath>
#include <sstream>
#include <cctype>
#include <algorithm>

// Helpers for move generation
static inline bool isWhitePieceChar(char c) {
    return std::isupper(static_cast<unsigned char>(c)) != 0;
}

static inline bool isBlackPieceChar(char c) {
    return std::islower(static_cast<unsigned char>(c)) != 0;
}

static inline bool isFriendly(char pieceChar, char color) {
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(color)));
    return (c == 'w') ? isWhitePieceChar(pieceChar) : isBlackPieceChar(pieceChar);
}

static inline bool isEnemy(char pieceChar, char color) {
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(color)));
    return (c == 'w') ? isBlackPieceChar(pieceChar) : isWhitePieceChar(pieceChar);
}

static inline ChessPiece pieceFromStateChar(char c) {
    switch (std::tolower(static_cast<unsigned char>(c))) {
        case 'p': return Pawn;
        case 'n': return Knight;
        case 'k': return King;
        default:  return NoPiece;
    }
}

Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag() - 128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    // Tag encodes type (1..6) and color (black flag = 128)
    int tag = static_cast<int>(piece);
    if (playerNumber != 0) tag |= 128;
    bit->setGameTag(tag);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");

    // Standard start position (works with board-only or full FEN)
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    startGame();
}

void Chess::FENtoBoard(const std::string& fen)
{
    // 1) Clear any existing pieces
    _grid->forEachSquare([](ChessSquare* square, int, int) {
        if (square && square->bit()) square->destroyBit();
    });

    // 2) Take only the piece placement field (first space-delimited token)
    std::string placement;
    {
        std::istringstream iss(fen);
        iss >> placement;
    }
    if (placement.empty()) return;

    // 3) Parse ranks 8 -> 1, left to right
    // In this project, y=0 is the TOP of the board, so rank 8 maps to y=0.
    int x = 0;
    int y = 0;

    for (char c : placement)
    {
        if (c == '/') {
            y++;
            x = 0;
            if (y >= 8) break;
            continue;
        }

        if (c >= '1' && c <= '8') {
            x += (c - '0');
            continue;
        }

        ChessPiece piece = NoPiece;
        int playerNumber = 0; // 0 = white, 1 = black

        switch (c)
        {
            // White
            case 'P': piece = Pawn;   playerNumber = 0; break;
            case 'N': piece = Knight; playerNumber = 0; break;
            case 'B': piece = Bishop; playerNumber = 0; break;
            case 'R': piece = Rook;   playerNumber = 0; break;
            case 'Q': piece = Queen;  playerNumber = 0; break;
            case 'K': piece = King;   playerNumber = 0; break;

            // Black
            case 'p': piece = Pawn;   playerNumber = 1; break;
            case 'n': piece = Knight; playerNumber = 1; break;
            case 'b': piece = Bishop; playerNumber = 1; break;
            case 'r': piece = Rook;   playerNumber = 1; break;
            case 'q': piece = Queen;  playerNumber = 1; break;
            case 'k': piece = King;   playerNumber = 1; break;

            default:
                break;
        }

        if (piece != NoPiece && x >= 0 && x < 8 && y >= 0 && y < 8)
        {
            ChessSquare* square = _grid->getSquare(x, 7 - y);
            if (square) {
                Bit* b = PieceForPlayer(playerNumber, piece);
                // Use dropBitAtPoint so the sprite snaps to the square correctly
                square->dropBitAtPoint(b, ImVec2(0, 0));
            }
        }

        x++;
        if (x > 8) x = 8;
    }
}

// Move generation (pawns, knights, king only)
std::vector<BitMove> Chess::generateMoves(const char* state, char color)
{
    std::vector<BitMove> moves;
    moves.reserve(40);
    if (!state) return moves;

    // Build a bitboard of friendly occupancy (for filtering attacks)
    uint64_t friendly = 0ULL;
    for (int i = 0; i < 64; i++) {
        const char pc = state[i];
        if (pc == '0') continue;
        if (isFriendly(pc, color)) friendly |= (1ULL << i);
    }

    // Precompute attack tables once
    static uint64_t KNIGHT_ATK[64];
    static uint64_t KING_ATK[64];
    static bool tablesInit = false;

    if (!tablesInit) {
        const uint64_t notA  = 0xfefefefefefefefeULL;
        const uint64_t notH  = 0x7f7f7f7f7f7f7f7fULL;
        const uint64_t notAB = 0xfcfcfcfcfcfcfcfcULL;
        const uint64_t notGH = 0x3f3f3f3f3f3f3f3fULL;

        for (int sq = 0; sq < 64; sq++) {
            const uint64_t b = 1ULL << sq;

            uint64_t n = 0ULL;
            n |= (b << 17) & notA;
            n |= (b << 15) & notH;
            n |= (b << 10) & notAB;
            n |= (b << 6)  & notGH;
            n |= (b >> 17) & notH;
            n |= (b >> 15) & notA;
            n |= (b >> 10) & notGH;
            n |= (b >> 6)  & notAB;
            KNIGHT_ATK[sq] = n;

            uint64_t k = 0ULL;
            k |= (b << 8);
            k |= (b >> 8);
            k |= (b << 1) & notA;
            k |= (b >> 1) & notH;
            k |= (b << 9) & notA;
            k |= (b << 7) & notH;
            k |= (b >> 7) & notA;
            k |= (b >> 9) & notH;
            KING_ATK[sq] = k;
        }

        tablesInit = true;
    }

    const bool isWhite = (std::tolower(static_cast<unsigned char>(color)) == 'w');

    // Board coordinates in your project: y=0 is the BOTTOM.
    // White moves toward higher y, black moves toward lower y.
    const int pawnDir = isWhite ? 1 : -1;

    // White pawns start on rank 2 => y=1
    // Black pawns start on rank 7 => y=6
    const int pawnStartRank = isWhite ? 1 : 6;

    for (int from = 0; from < 64; from++) {
        const char pc = state[from];
        if (pc == '0') continue;
        if (!isFriendly(pc, color)) continue;

        const ChessPiece p = pieceFromStateChar(pc);
        const int fx = from % 8;
        const int fy = from / 8;

        // PAWNS
        if (p == Pawn) {
            // forward one
            const int ny = fy + pawnDir;
            if (ny >= 0 && ny < 8) {
                const int one = ny * 8 + fx;
                if (state[one] == '0') {
                    moves.emplace_back(from, one, Pawn);

                    // forward two from starting rank (must have empty one and empty two)
                    if (fy == pawnStartRank) {
                        const int nny = fy + 2 * pawnDir;
                        if (nny >= 0 && nny < 8) {
                            const int two = nny * 8 + fx;
                            if (state[two] == '0') {
                                moves.emplace_back(from, two, Pawn);
                            }
                        }
                    }
                }
            }

            // captures diagonally
            const int cy = fy + pawnDir;
            if (cy >= 0 && cy < 8) {
                if (fx - 1 >= 0) {
                    const int to = cy * 8 + (fx - 1);
                    if (isEnemy(state[to], color)) moves.emplace_back(from, to, Pawn);
                }
                if (fx + 1 < 8) {
                    const int to = cy * 8 + (fx + 1);
                    if (isEnemy(state[to], color)) moves.emplace_back(from, to, Pawn);
                }
            }

            continue;
        }

        // KNIGHTS
        if (p == Knight) {
            BitboardElement atk(KNIGHT_ATK[from] & ~friendly);
            atk.forEachBit([&](int to) {
                moves.emplace_back(from, to, Knight);
            });
            continue;
        }

        // KING
        if (p == King) {
            BitboardElement atk(KING_ATK[from] & ~friendly);
            atk.forEachBit([&](int to) {
                moves.emplace_back(from, to, King);
            });
            continue;
        }
    }

    return moves;
}

void Chess::clearBoardHighlights()
{
    // Call base version first
    Game::clearBoardHighlights();

    // Force-clear our ChessSquare highlight flag (this is what draws yellow)
    _grid->forEachSquare([](ChessSquare* square, int, int) {
        if (square) square->setHighlighted(false);
    });

    _highlightsActive = false;
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    clearBoardHighlights();
    return false;
}

static std::string boardPrettyFromState(const std::string& s)
{
    std::string out;
    out.reserve(8 * 9);

    // Print top row first for humans (rank 8 down to 1)
    for (int y = 7; y >= 0; y--) {
        for (int x = 0; x < 8; x++) {
            out += s[y * 8 + x];
        }
        out += '\n';
    }
    return out;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // Clear old highlights
    clearBoardHighlights();
    _highlightsActive = false;
    
    // Only allow selecting your own pieces
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128; // 0 = white, 128 = black
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor != currentPlayer) return false;

    // Figure out which square we are moving from
    auto* srcSq = dynamic_cast<ChessSquare*>(&src);
    if (!srcSq) return true; // allow drag, just no highlights

    const int from = srcSq->getSquareIndex();
    const char color = (getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';

    // Generate moves for current position
    std::string s = stateString();
    std::vector<BitMove> moves = generateMoves(s.c_str(), color);

    // VS Code Debug Console output (run with debugger)
    std::cout << "\n=== MoveGen Debug ===\n";
    std::cout << "Player color: " << color << "\n";
    std::cout << "Board:\n" << boardPrettyFromState(s);
    std::cout << "Total moves: " << moves.size() << "\n";

    // show first 20 moves (assignment wants 20)
    for (int i = 0; i < (int)moves.size() && i < 20; i++) {
        std::cout << i << ": " << moves[i].from << " -> " << moves[i].to
                  << " piece=" << (int)moves[i].piece << "\n";
    }
    std::cout << "=====================\n";
    std::cout << std::flush;

    // Highlight destination squares for this piece
    bool anyHighlighted = false;
    for (const BitMove& m : moves) {
        if (m.from == from) {
            int tx = m.to % 8;
            int ty = m.to / 8;
            auto* dstSq = _grid->getSquare(tx, ty);
            if (dstSq) {
                dstSq->setHighlighted(true);
                anyHighlighted = true;
            }
        }
    }
_highlightsActive = anyHighlighted;
    return true;
}

void Chess::drawFrame()
{
    // Run base drawing + input handling
    Game::drawFrame();

    // If we have highlights up, but we're not currently dragging anything,
    // it means the piece got "deselected"/cancelled without a callback.
    if (_highlightsActive && _dragBit == nullptr) {
        clearBoardHighlights();
    }
}

bool Chess::clickedBit(Bit &bit)
{
    // Clicking (without dragging) should clear any old move highlights.
    clearBoardHighlights();
    return true; // allow the click
}

void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    // If we moved onto an occupied square, capture it.
    // We want to destroy ONLY the piece that belonged to the other player.

    Bit* dstBit = dst.bit();

    if (dstBit && dstBit != &bit) {
        // There was already a piece here -> capture
        pieceTaken(dstBit);     // optional hook in Game
        dst.destroyBit();       // remove captured piece
    }

    // Turn is over after one move
    clearBoardHighlights();
    endTurn();
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    auto* srcSq = dynamic_cast<ChessSquare*>(&src);
    auto* dstSq = dynamic_cast<ChessSquare*>(&dst);
    if (!srcSq || !dstSq) return false;

    const int from = srcSq->getSquareIndex();
    const int to   = dstSq->getSquareIndex();

    const char color = (getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';

    const std::string s = stateString();
    std::vector<BitMove> moves = generateMoves(s.c_str(), color);

    _lastMoves = moves;
    _lastFrom = from;

    const ChessPiece p = static_cast<ChessPiece>(bit.gameTag() & 0x7F);
    bool ok = std::any_of(moves.begin(), moves.end(), [&](const BitMove& m) {
        return m.from == from && m.to == to && m.piece == static_cast<uint8_t>(p);
    });

    if (!ok) {
        clearBoardHighlights();
        _highlightsActive = false;
    }
    return ok;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int, int) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);

    // State index is y*8+x, where y=0 is the BOTTOM row in this project.
    // This must match getSquare(x,y) and getSquareIndex().
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            s += pieceNotation(x, y);
        }
    }

    return s;
}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}
