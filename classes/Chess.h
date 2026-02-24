#pragma once

#include "Game.h"
#include "Grid.h"

#include <vector>
#include "Bitboard.h"

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;
    void bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool clickedBit(Bit &bit) override;
    void drawFrame() override;
    void clearBoardHighlights() override;

    Grid* getGrid() override { return _grid; }
    std::vector<BitMove> generateMoves(const char* state, char color);
    
private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;

    Grid* _grid;

    std::vector<BitMove> _lastMoves;
    int _lastFrom = -1;

    bool _highlightsActive = false;
};