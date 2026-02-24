# Chess Project

This project is a playable Chess implementation built using the provided game engine. The board is now set up using FEN notation, which allows the game to correctly load the standard starting position and other board states.

<img width="851" height="764" alt="image" src="https://github.com/user-attachments/assets/e144221f-ae5b-4069-9842-3a1470bd50f4" />

I implemented a move generator using bitboards. Right now it supports pawns (single move, double move, and captures), knights, and kings. Legal moves are highlighted in yellow when you select a piece, and the highlights now correctly disappear when the move is completed, cancelled, or the turn ends.

Move generation also prints debug output to the VS Code Debug Console so I can verify the number of generated moves and check correctness.

<img width="224" height="628" alt="image" src="https://github.com/user-attachments/assets/2648735c-1a19-4aa4-bce9-2a1dcff2fa61" />

Sliding pieces, check/checkmate, castling, and special rules are not implemented yet, but the core move validation and board logic are working.