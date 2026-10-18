CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lncurses

all: snake

snake: snake.c
	$(CC) $(CFLAGS) snake.c -o snake $(LDFLAGS)

clean:
	rm -f snake

.PHONY: all clean
