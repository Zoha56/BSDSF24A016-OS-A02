CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SRC = src/lsv1.0.0.c
OBJ = obj/lsv1.0.0.o
TARGET = bin/ls

all: $(TARGET)

$(TARGET): $(OBJ) | bin
	$(CC) $(CFLAGS) -o $@ $(OBJ)

$(OBJ): $(SRC) | obj
	$(CC) $(CFLAGS) -c $< -o $@

bin obj:
	mkdir -p $@

clean:
	rm -rf obj bin

.PHONY: all clean
