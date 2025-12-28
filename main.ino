#include <string.h>
#include <ctype.h>
#include <LiquidCrystal.h>

const int selectPins[3] = {3, 4, 5};
const int colPins[8] = {6, 7, 8, 9, 10, 11, 12, 13}; // columns A - H

// pins for LCD 1602
const uint8_t LCD_RS = A0;
const uint8_t LCD_E  = A1;
const uint8_t LCD_D4 = A2;
const uint8_t LCD_D5 = A3;
const uint8_t LCD_D6 = A4;
const uint8_t LCD_D7 = A5;

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

const int STABILITY_READS = 4; // number of identical board scans needed

struct Piece {
  char colour;
  char type;
};

struct Square { int rank, file; };

Piece board[8][8]; // board[rank][file]
bool sensors[8][8]; 
bool prevSensors[8][8];

struct MoveInfo {
  int fromR, fromF, toR, toF;
  char pieceColour;
  char pieceType;
  bool doublePawnPush;
} lastMove; // tracked for en passant
int moveCount = 0;

char sideToMove = 'w';

bool whiteKingMoved = false;
bool blackKingMoved = false;
bool whiteRookAMoved = false;
bool whiteRookHMoved = false;
bool blackRookAMoved = false;
bool blackRookHMoved = false;

int halfmove = 0;
int fullmove = 1;

// en passant tracking
int epRow = -1;
int epCol = -1;

const char *FEN_PREFIX = "FEN:";
const char *MOVE_PREFIX = "MOVE:";

String incomingBuf = "";

// Helper functions
int fileCharToFile(char f) { return (f >= 'a' && f <= 'h') ? (f - 'a') : -1; }
int rankCharToRank(char r) { return (r >= '1' && r <= '8') ? (r - '1') : -1; }

inline bool inBounds(int rank, int file) { return (rank >= 0 && rank < 8 && file >= 0 && file < 8); }

void selectRow(int row) {
  for (int bit = 0; bit < 3; bit++) {
    digitalWrite(selectPins[bit], (row >> bit) & 1);
  }
  delayMicroseconds(60);
}

bool compareBoards(bool a[8][8], bool b[8][8]) {
  for (int rank = 0; rank < 8; rank++)
    for (int file = 0; file < 8; file++)
      if (a[rank][file] != b[rank][file]) return false;
  return true;
}

bool getEnPassantTarget(int &outR, int &outF) {
  if (!lastMove.doublePawnPush) return false;
  if (lastMove.pieceColour == 'w') outR = lastMove.toR - 1;
  else outR = lastMove.toR + 1;
  outF = lastMove.toF;
  if (!inBounds(outR, outF)) return false;
  return true;
}

void updateGameStateAfterMove(int fromR, int fromF, int toR, int toF, Piece moved, bool wasCapture, bool wasEnPassant, bool isCastling) {
  if (moved.type == 'P' || wasCapture) halfmove = 0;
  else halfmove++;

  sideToMove = (sideToMove == 'w') ? 'b' : 'w';
  if (sideToMove == 'w') fullmove++;

  epRow = -1;
  epCol = -1;

  if (isCastling) {
    if (fromR == 0) {
      whiteRookAMoved = true;
      whiteRookHMoved = true;
    }
    else if (fromR == 7) {
      blackRookAMoved = true;
      blackRookHMoved = true;
    }
    return;
  }

  // detect changes in castling flags
  if (moved.type == 'P' && abs(toR - fromR) == 2) {
    epRow = (fromR + toR) / 2;
    epCol = fromF;
  }

  if (moved.type == 'K') {
    if (moved.colour == 'w') whiteKingMoved = true;
    else blackKingMoved = true;
  }

  if (moved.type == 'R') {
    if (moved.colour == 'w') {
      if (fromR == 0 && fromF == 0) whiteRookAMoved = true;
      if (fromR == 0 && fromF == 7) whiteRookHMoved = true;
    }
    else {
      if (fromR == 7 && fromF == 0) blackRookAMoved = true;
      if (fromR == 7 && fromF == 7) blackRookHMoved = true;
    }
  }

  if (wasCapture) {
    if (toR == 0 && toF == 0) whiteRookAMoved = true;
    if (toR == 0 && toF == 7) whiteRookHMoved = true;
    if (toR == 7 && toF == 0) blackRookAMoved = true;
    if (toR == 7 && toF == 7) blackRookHMoved = true;
  }
}

String getCastlingFEN() {
  String s = "";

  if (!whiteKingMoved && !whiteRookHMoved) s += "K";
  if (!whiteKingMoved && !whiteRookAMoved) s += "Q";
  if (!blackKingMoved && !blackRookHMoved) s += "k";
  if (!blackKingMoved && !blackRookAMoved) s += "q";

  if (s.length() == 0) s = "-";
  return s;
}

String getEnPassantFEN() {
  if (epRow == -1) return "-";

  char file = 'a' + epCol;
  char rank = '1' + epRow;
  String s = "";
  s += file;
  s += rank;
  return s;
}

void applyUciMoveToBoard(const String &uci) {
  if (uci.length() < 4) return;
  char f1 = uci.charAt(0);
  char r1 = uci.charAt(1);
  char f2 = uci.charAt(2);
  char r2 = uci.charAt(3);
  int file1 = fileCharToFile(f1), row1 = rankCharToRank(r1);
  int file2 = fileCharToFile(f2), row2 = rankCharToRank(r2);
  if (!(file1 >= 0 && file2 >= 0 && row1 >= 0 && row2 >= 0)) return;

  Piece moving = board[row1][file1];
  Piece target = board[row2][file2];

  if (moving.type == 'K' && row1 == row2 && abs(file2 - file1) == 2) {
    if (file2 > file1) {
      board[row2][file2] = moving;
      board[row1][file1].colour = ' ';
      board[row1][file1].type = ' ';
      board[row2][5] = board[row2][7];
      board[row2][7].colour = ' ';
      board[row2][7].type = ' ';
    }
    else {
      board[row2][file2] = moving;
      board[row1][file1].colour = ' ';
      board[row1][file1].type = ' ';
      board[row2][3] = board[row2][0];
      board[row2][0].colour = ' ';
      board[row2][0].type = ' ';
    }
    return;
  }

  if (moving.type == 'P' && file1 != file2 && target.colour == ' ') {
    int capR = (moving.colour == 'w') ? (row2 - 1) : (row2 + 1);
    int capF = file2;
    if (capR >= 0 && capR < 8) {
      board[capR][capF].colour = ' ';
      board[capR][capF].type = ' ';
    }
  }

  // promotion
  if (uci.length() >= 5 && moving.type == 'P') {
    char prom = uci.charAt(4);
    board[row2][file2].colour = moving.colour;
    board[row2][file2].type = (char)toupper(prom);
    board[row1][file1].colour = ' ';
    board[row1][file1].type  = ' ';
    return;
  }

  // normal move or capture
  board[row2][file2] = moving;
  board[row1][file1].colour = ' ';
  board[row1][file1].type = ' ';
}

// gets lists of added and removed pieces
void getDiffs(Square added[], int &addedCount, Square removed[], int &removedCount) {
  addedCount = removedCount = 0;
  for (int rank = 0; rank < 8; rank++) {
    for (int file = 0; file < 8; file++) {
      if (addedCount < 8 && sensors[rank][file] && !prevSensors[rank][file]) added[addedCount++] = {rank, file};
      if (removedCount < 8 && !sensors[rank][file] && prevSensors[rank][file]) removed[removedCount++] = {rank, file};
    }
  }
  return;
}

void setupBoard() {
  for (int rank = 0; rank < 8; rank++)
    for (int file = 0; file < 8; file++) {
      board[rank][file].colour = ' ';
      board[rank][file].type = ' ';
    }

  // white pieces
  board[0][0] = {'w', 'R'};
  board[0][1] = {'w', 'N'};
  board[0][2] = {'w', 'B'};
  board[0][3] = {'w', 'Q'};
  board[0][4] = {'w', 'K'};
  board[0][5] = {'w', 'B'};
  board[0][6] = {'w', 'N'};
  board[0][7] = {'w', 'R'};
  for (int file = 0; file < 8; file++) board[1][file] = {'w', 'P'};

  // black pieces
  board[7][0] = {'b', 'R'};
  board[7][1] = {'b', 'N'};
  board[7][2] = {'b', 'B'};
  board[7][3] = {'b', 'Q'};
  board[7][4] = {'b', 'K'};
  board[7][5] = {'b', 'B'};
  board[7][6] = {'b', 'N'};
  board[7][7] = {'b', 'R'};
  for (int file = 0; file < 8; file++) board[6][file] = {'b', 'P'};

  Serial.println("Starting position loaded:");
}

// Debugging / Output
void printBoard() {
  Serial.println();
  Serial.println("  +-----------------------------------------+");
  for (int rank = 7; rank >= 0; rank--) {
    Serial.print(rank + 1);
    Serial.print(" |");
    for (int file = 0; file < 8; file++) {
      Piece p = board[rank][file];
      if (p.colour == ' ') { Serial.print(" .. "); }
      else {
        Serial.print(' ');
        Serial.print(p.colour);
        Serial.print(p.type);
        Serial.print(' ');
      }
    }
    Serial.println("|");
  }
  Serial.println("  +-----------------------------------------+");
  Serial.println("    A   B   C   D   E   F   G   H");
  Serial.println();
}

void printSquareName(Square s) {
  char file = 'A' + s.file;
  int rank = s.rank + 1;
  Serial.print(file);
  Serial.print(rank);
}

// prints a bitboard of all occupied squares
void printOccupiedSquares(bool bb[8][8]) {
  Serial.println("Sensor snapshot (rank 8 -> 1):");
  for (int rank = 7; rank >= 0; rank--) {
    Serial.print(rank + 1);
    Serial.print(": ");
    for (int file = 0; file < 8; file++) {
      Serial.print(bb[rank][file] ? "1" : "0");
    }
    Serial.println();
  }
}

void handleIncomingSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (incomingBuf.length() > 0) {
        if (incomingBuf.startsWith(MOVE_PREFIX)) {
          String payload = incomingBuf.substring(strlen(MOVE_PREFIX));
          payload.trim();
          Serial.print("Received engine move: ");
          Serial.println(payload);

          showEnginePlayed(payload);
          applyUciMoveToBoard(payload);
          printBoard();
        }
      }
      incomingBuf = "";
    }
    else {
      incomingBuf += c;
      if (incomingBuf.length() > 200) incomingBuf = "";
    }
  }
}

// Handling movement / sensors
void readSensors(bool dest[8][8]) {
  for (int rank = 0; rank < 8; rank++) {
    selectRow(rank);
    for (int file = 0; file < 8; file++) {
      int val = digitalRead(colPins[file]);
      dest[rank][file] = !val; // sensor is active-low
    }
    delayMicroseconds(60);
  }
}

void handleSensorChange() {
  Square added[8], removed[8];
  int addedCount, removedCount;
  getDiffs(added, addedCount, removed, removedCount);

  Serial.print("Detected change: added=");
  Serial.print(addedCount);
  Serial.print(" removed=");
  Serial.println(removedCount);

  if (removedCount == 1 && addedCount == 1) {
    handleMove(removed[0], added[0]);
  }
  else if (removedCount == 2 && addedCount == 2) handleCastling(removed, added);
  else if (removedCount == 1 && addedCount == 0) {
    Serial.println("WARNING: piece removed but no new piece detected.");
    board[removed[0].rank][removed[0].file].colour = ' ';
    board[removed[0].rank][removed[0].file].type = ' ';
  }
  else if (removedCount == 0 && addedCount == 1) {
    Serial.println("WARNING: piece placed but no removal detected. Assumed white pawn by default.");
    board[added[0].rank][added[0].file].colour = 'w';
    board[added[0].rank][added[0].file].type = 'P';
  }
  return;
}

void makeMove(bool from[8][8], bool to[8][8]) {
  for (int rank = 0; rank < 8; rank++)
    for (int file = 0; file < 8; file++)
      to[rank][file] = from[rank][file];
}

void handleMove(Square fromSq, Square toSq) {
  Piece moving = board[fromSq.rank][fromSq.fjle];
  Piece target = board[toSq.rank][toSq.file];

  bool isCapture = (target.colour != ' ' && target.colour != moving.colour);
  bool enPassantCapture = false;

  // check for en passant
  if (moving.type == 'P' && board[toSq.rank][toSq.file].colour == ' ' && fromSq.file != toSq.file) {
    if (lastMove.doublePawnPush && lastMove.toR == fromSq.rank && lastMove.toF == toSq.file && lastMove.pieceType == 'P' && lastMove.pieceColour != moving.colour) {
      int capturedPawnR = lastMove.toR;
      int capturedPawnF = lastMove.toF;
      Serial.println("En passant capture detected.");
      board[capturedPawnR][capturedPawnF].colour = ' ';
      board[capturedPawnR][capturedPawnF].type = ' ';
      enPassantCapture = true;
      isCapture = true;
    }
  }

  board[toSq.rank][toSq.file] = moving;
  board[fromSq.rank][fromSq.file].colour = ' ';
  board[fromSq.rank][fromSq.file].type = ' ';

  // check for promotion
  if (moving.type == 'P') {
    if ((moving.colour == 'w' && toSq.rank == 7) || (moving.colour == 'b' && toSq.rank == 0)) {
      board[toSq.rank][toSq.file].type = 'Q';
      Serial.print("Pawn promoted to Queen at ");
      printSquareName(toSq);
      Serial.println(".");
    }
  }

  updateGameStateAfterMove(fromSq.rank, fromSq.file, toSq.rank, toSq.file, moving, isCapture, enPassantCapture, false);
  lastMove.fromR = fromSq.rank; lastMove.fromF = fromSq.file;
  lastMove.toR = toSq.rank; lastMove.toF = toSq.file;
  lastMove.pieceColour = moving.colour;
  lastMove.pieceType = moving.type;
  lastMove.doublePawnPush = (moving.type == 'P' && abs(toSq.rank - fromSq.rank) == 2);

  moveCount++;

  Serial.print("Move ");
  Serial.print(moveCount);
  Serial.print(": ");
  printSquareName(fromSq);
  Serial.print(" -> ");
  printSquareName(toSq);
  Serial.print(isCapture ? "  (capture)" : "");
  if (enPassantCapture) Serial.print("  (en passant)");
  Serial.println();
  return;
}

void handleCastling(Square removed[], Square added[]) {
  Piece p1 = board[removed[0].rank][removed[0].file];
  Piece p2 = board[removed[1].rank][removed[1].file];

  bool p1_isK = (p1.type == 'K');
  bool p2_isK = (p2.type == 'K');
  bool p1_isR = (p1.type == 'R');
  bool p2_isR = (p2.type == 'R');

  if (!((p1_isK && p2_isR) || (p2_isK && p1_isR))) return;

  Square kingFrom = p1_isK ? removed[0] : removed[1];
  Square rookFrom = p1_isR ? removed[0] : removed[1];
  char colour = board[kingFrom.rank][kingFrom.file].colour;

  for (int a = 0; a < 2; a++) {
    Square kingTo = added[a];
    Square rookTo = added[1 - a];

    if (kingFrom.rank != kingTo.rank) continue;
    int dFile = kingTo.file - kingFrom.file;
    if (!(abs(dFile) == 2)) continue;

    // rook should be one square across from king on opposite side 
    int expectedRookF = (dFile > 0) ? kingTo.file - 1 : kingTo.file + 1;
    if (rookTo.rank == kingFrom.rank && rookTo.file == expectedRookF) {
      Piece kingPiece = board[kingFrom.rank][kingFrom.file];
      Piece rookPiece = board[rookFrom.rank][rookFrom.file];

      board[kingTo.rank][kingTo.file] = kingPiece;
      board[rookTo.rank][rookTo.file] = rookPiece;
      board[kingFrom.rank][kingFrom.file].colour = ' ';
      board[kingFrom.rank][kingFrom.file].type = ' ';
      board[rookFrom.rank][rookFrom.file].colour = ' ';
      board[rookFrom.rank][rookFrom.file].type = ' ';

      updateGameStateAfterMove(kingFrom.rank, kingFrom.file, kingTo.rank, kingTo.file, kingPiece, false, false, true);

      lastMove.fromR = kingFrom.rank;
      lastMove.fromF = kingFrom.file;
      lastMove.toR = kingTo.rank;
      lastMove.toF = kingTo.file;
      lastMove.pieceColour = colour;
      lastMove.pieceType = 'K';
      lastMove.doublePawnPush = false;
      moveCount++;

      Serial.print("Castling detected for colour ");
      Serial.print(colour == 'w' ? "White" : "Black");
      Serial.print(": King ");
      printSquareName(kingFrom);
      Serial.print(" -> ");
      printSquareName(kingTo);
      Serial.print(", Rook "); 
      printSquareName(rookFrom);
      Serial.print(" -> ");
      printSquareName(rookTo);
      Serial.println();
      return;
    }
  }

  return;
}

// Engine
void showEnginePlayed(const String &moveStr) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Engine played: ");
  lcd.setCursor(0,1);
  String msg = moveStr;
  if (msg.length() > 16) msg = msg.substring(0,16);
  lcd.print(msg);
  Serial.print("Engine played: ");
  Serial.println(moveStr);
}

void sendFENToPython(const String &fen) {
  String line = String(FEN_PREFIX) + fen + "\n";
  Serial.print(line);
  Serial.print("Sent FEN: ");
  Serial.println(fen);
}

String genFEN() {
  String fen = "";
  for (int rank = 7; rank >= 0; rank--) {
    int empty = 0;
    for (int file = 0; file < 8; file++) {
      Piece p = board[rank][file];
      if (p.colour == ' ') empty++;
      else {
        if (empty > 0) {
          fen += String(empty);
          empty = 0;
        }
        char letter = p.type;
        if (p.colour == 'w') fen += String(letter);
        else fen += String((char)tolower(letter));
      }
    }
    if (empty > 0) fen += String(empty);
    if (rank > 0) fen += '/';
  }

  fen += " ";
  fen += sideToMove;
  fen += " ";
  fen += getCastlingFEN();
  fen += " ";
  fen += getEnPassantFEN();
  fen += " ";
  fen += String(halfmove);
  fen += " ";
  fen += String(fullmove);
  return fen;
}

bool loadFEN(const char *fen) {
  if (!fen || fen[0] == '\0') return false;

  char buf[128];
  strncpy(buf, fen, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *parts[6] = {0};
  int p = 0;
  char *tok = strtok(buf, " ");
  while (tok && p < 6) {
    parts[p++] = tok;
    tok = strtok(NULL, " ");
  }
  if (p < 4) return false;

  char *placement = parts[0];
  char *active = parts[1];
  char *castling = parts[2];
  char *ep = parts[3];
  halfmove = (p > 4) ? parts[4] : NULL;
  fullmove = (p > 5) ? parts[5] : NULL;

  for (int rank = 0; rank < 8; rank++) {
    for (int file = 0; file < 8; file++) {
      board[rank][file].colour = ' ';
      board[rank][file].type = ' ';
    }
  }

  int rank = 7;
  int file = 0;
  for (const char *q = placement; *q; q++) {
    char ch = *q;
    if (ch == '/') {
      rank--;
      file = 0;
      if (rank < 0) return false;
      continue;
    }
    if (isdigit(ch)) {
      file += ch - '0';
      if (file > 8) return false;
      continue;
    }
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return false;
    Piece piece;
    if (isupper(ch)) {
      piece.colour = 'w';
      piece.type = ch;
    }
    else {
      piece.colour = 'b';
      piece.type = toupper(ch);
    }
    board[rank][file] = piece;
    file++;
  }

  sideToMove = activeColour;
  halfmove = (halfmove != NULL) ? atoi(halfmove) : 0;
  fullmove = (fullmove != NULL) ? atoi(fullmove) : 1;
  if (active && active[0] == 'b') activeColour = 'b';
  if (castling && castling[0] != '-') {
    for (char *c = castling; *c; c++) {
      if (*c == 'K') whiteKingMoved = false;
      else if (*c == 'Q') whiteKingMoved = false;
      else if (*c == 'k') blackKingMoved = false;
      else if (*c == 'q') blackKingMoved = false;
    }
  }

  lastMove.doublePawnPush = false;
  lastMove.fromR = lastMove.fromF = lastMove.toR = lastMove.toF = -1;
  lastMove.pieceType = ' ';
  lastMove.pieceColour = ' ';

  if (ep && ep[0] != '-') {
    if (strlen(ep) >= 2 && ep[0] >= 'a' && ep[0] <= 'h' && ep[1] >= '1' && ep[1] <= '8') {
      int epCol = ep[0] - 'a';
      int epRow = ep[1] - '1';
      char lastMoverColour = (activeColour == 'b') ? 'w' : 'b';
      int pawnLandingRow;
      if (lastMoverColour == 'w') pawnLandingRow = epRow + 1;
      else pawnLandingRow = epRow - 1;
      if (pawnLandingRow >= 0 && pawnLandingRow <= 7) {
        lastMove.doublePawnPush = true;
        lastMove.toR = pawnLandingRow;
        lastMove.toF = epCol;
        lastMove.pieceColour = lastMoverColour;
        lastMove.pieceType = 'P';
      }
    }
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.clear();
  Serial.println("Chessboard scanner starting...");

  for (int i = 0; i < 3; i++) {
    pinMode(selectPins[i], OUTPUT);
    digitalWrite(selectPins[i], LOW);
  }
  for (int i = 0; i < 8; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }

  setupBoard();
  printBoard();

  for (int rank = 0; rank < 8; rank++) {
    for (int file = 0; file < 8; file++) {
      sensors[rank][file] = false;
      prevSensors[rank][file] = false;
    }
  }

  lastMove.fromR = lastMove.fromF = lastMove.toR = lastMove.toF = -1;
  lastMove.pieceColour = ' ';
  lastMove.pieceType = ' ';
  lastMove.doublePawnPush = false;

  delay(1000);
}

void loop() {
  handleIncomingSerial();
  bool stable = false;
  bool tempBoard[8][8];

  for (int attempt = 0; attempt < STABILITY_READS; attempt++) {
    readSensors(tempBoard);

    if (attempt == 0) { makeMove(tempBoard, sensors); }
    else if (!compareBoards(sensors, tempBoard)) {
      attempt = =-1;
      makeMove(tempBoard, sensors);
      delay(60);
      continue;
    }
    delay(60);
    if (attempt == STABILITY_READS - 1) stable = true;
  }
  if (!stable) return;

  if (!compareBoards(sensors, prevSensors)) {
    handleSensorChange();
    printBoard();
    makeMove(sensors, prevSensors);
  }

  delay(60);
}