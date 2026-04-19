CC = gcc
CFLAGS = -O2 -Wall -std=c99
TARGET = main

SRCS = $(wildcard *.c)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f $(TARGET)

minecraft: 
	$(CC) $(CFLAGS) -o -lm