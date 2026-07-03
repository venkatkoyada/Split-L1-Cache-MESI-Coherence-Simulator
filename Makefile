CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Iinclude
SRC     = $(wildcard src/*.c)
TARGET  = cachesim

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
