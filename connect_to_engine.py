import serial
import time
import sys
import chess
import chess.engine

# Config
SERIAL_PORT = "/dev/cu.usbmodem101" # replace with your serial port
BAUDRATE = 115200
STOCKFISH_PATH = "/path/to/stockfish" # replace with your path
ENGINE_TIME_LIMIT = 1

try:
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout = 0.1)
except Exception as e:
    print("Failed to open serial port:", e)
    sys.exit(1)
print("Opened serial", SERIAL_PORT, "at", BAUDRATE)

try:
    engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)
except Exception as e:
    print("Failed to open Stockfish:", e)
    ser.close()
    sys.exit(1)
print("Stockfish started...")

buffer = ""

def send_move_to_arduino(uci):
    line = "MOVE:" + uci + "\n"
    ser.write(line.encode('utf-8'))
    print("Sent to Arduino:", line.strip())

try:
    while True:
        data = ser.read(1024)
        if data:
            s = data.decode('utf-8', errors='ignore')
            buffer += s
            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                line = line.strip()
                if not line:
                    continue
                if line.startswith("FEN:"):
                    fen = line[4:].strip()
                    try:
                        board = chess.Board(fen)
                    except Exception as e:
                        print("Invalid FEN received:", fen, e)
                        continue
                    try:
                        result = engine.play(board, chess.engine.Limit(time = ENGINE_TIME_LIMIT))
                        if result and result.move:
                            uci = result.move.uci()
                            send_move_to_arduino(uci)
                        else:
                            print("Engine returned no move")
                    except Exception as e:
                        print("Engine error:", e)
                else:
                    print("Ignored line:", line)
        time.sleep(0.05)
except KeyboardInterrupt:
    print("Exiting...")
finally:
    engine.quit()
    ser.close()
