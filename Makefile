CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = cache_sim

.PHONY: all clean run

all: $(TARGET)

$(TARGET): cache_sim.c
	$(CC) $(CFLAGS) -o $(TARGET) cache_sim.c

run: $(TARGET)
	./$(TARGET) config.txt trace.txt

clean:
	rm -f $(TARGET) output.txt
