CC=g++
CFLAGS=-O2 -std=c++17 `sdl2-config --cflags`
LDFLAGS=`sdl2-config --libs` -lSDL2_ttf -pthread
TARGET=pong
SRC=pong.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
