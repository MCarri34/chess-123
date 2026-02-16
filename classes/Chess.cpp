#include "Chess.h"
#include <limits>
#include <cmath>
#include <sstream>

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
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

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
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    startGame();
}

void Chess::FENtoBoard(const std::string& fen) {
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)
    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)

    // 1) Clear any existing pieces
    _grid->forEachSquare([](ChessSquare* square, int, int) {
        if (square && square->bit()) square->destroyBit();
    });

    // 2) Support either "board-only" FEN or full FEN:
    std::string placement;
    {
        std::istringstream iss(fen);
        iss >> placement;
    }
    if (placement.empty()) return;

    // 3) Parse ranks 8 -> 1 (top to bottom)
    int x = 0;
    int y = 0; // y=0 is rank 8

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
        int playerNumber = 0; // 0=white, 1=black

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
                // invalid character => ignore
                break;
        }

        if (piece != NoPiece && x >= 0 && x < 8 && y >= 0 && y < 8)
        {
            ChessSquare* square = _grid->getSquare(x, 7 - y);
            if (square) {
                Bit* b = PieceForPlayer(playerNumber, piece);
                square->dropBitAtPoint(b, ImVec2(0, 0));
            }
        }

        x++;
        if (x > 8) x = 8;
    }
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor == currentPlayer) return true;
    return false;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    return true;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
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
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
            s += pieceNotation( x, y );
        }
    );
    return s;}

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
