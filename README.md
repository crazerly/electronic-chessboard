# Electronic Chessboard
An electronic chessboard built using hall effect sensors for players to record OTB games or play against an engine.

## Materials
 - 64x hall effect sensors
 - 64x neodymium magnets
 - 8x ~500Ω resistors
 - 8x 2N3906 transistors
 - wire (enough for 24 strips the length of the board)
 - 8-channel demultiplexer
 - 1602 LCD screen
 - potentiometer (not required)
 - sheet of thin cardboard
 - Arduino Uno or other microcontroller
 - chess set

## Tools
 - soldering iron

## The pieces
Each chess piece originally contained a small plastic disk, which I replaced with a neodymium magnet to allow Hall effect sensors to detect it.
<div align="center">
  <img src="https://github.com/crazerly/electronic-chessboard/blob/main/imgs/IMG_0076.jpg" width="300"/>
</div>
<br />

## The chessboard
The chessboard was constructed from a thin cardboard sheet, with a 24x24cm grid drawn on. A hole is cut into each square for the Hall effect sensor to poke through.
The sensors output HIGH by default, and are pulled LOW when a magnet (chess piece) is placed above above the square.
<div align="center">
  <img src="https://github.com/crazerly/electronic-chessboard/blob/main/imgs/IMG_0077.jpg" width="500"/>
</div>
<br />

The board uses a read/write scanning approach to minimise the number of Arduino pins.
* A demux selects one rank (row) at a time.
* Each of the demux's outputs power a transistor that powers all sensors in that rank. 
* Eight column (file) wires read the sensor outputs for the currently activated rank.
and a wire for each column (file) receives the value for each sensor in that row.  
For example, if rank 1 is selected and the fourth column wire reads LOW the program interprets this a piece is on D1.  
<div align="center">
  <img src="https://github.com/crazerly/electronic-chessboard/blob/main/imgs/board_schematic.png" width="700"/>
</div>
<br />

<div align="center">
  <img src="https://github.com/crazerly/electronic-chessboard/blob/main/imgs/IMG_0647.jpg" width="700"/>
</div>
<br />

## The code
The program stores a complete internal representation of the chessboard using only the sensor positions and the initial board state.
The board is continuously scanned to detect changes between the previous and current board state, indicating a move.
### Features:
* The program identifies all types of moves including captures, en passant and castling.
* It can convert FEN strings to board states
* It can generate FEN strings for the current state including active colour, castling rights and en passant flags and move counters
* Serial communication with a Python program to integrate a chess engine using the 'chess' module
* LCD output of engine moves, allowing the board to be entirely standalone when playing against an engine without needing to check a computer screen

## Demo
<a href="https://www.youtube.com/watch?v=Z--Xj8hdzuM" align="center">
  <img src="https://github.com/crazerly/electronic-chessboard/blob/main/imgs/Electronic%20Chessboard.png" width="600">
</a>

## Further features
- [ ] Connection to Lichess

