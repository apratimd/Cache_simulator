# Compiler settings
CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11

# Targets and files
TARGET = cache_sim
SRC = cache_sim.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET) config.txt trace.txt

.PHONY: all clean run
