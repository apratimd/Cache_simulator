# High-Performance C Cache Simulator

A highly configurable, high-performance Cache Simulator written in C. It simulates a cache memory architecture based on custom layout parameters, processes memory access traces, and evaluates performance metrics like hit/miss rates, Average Memory Access Time (AMAT), and types of cache misses (Compulsory, Capacity, and Conflict).

---

## Features

- **Configurable Layout**: Easily configure cache size, block size, associativity, and address width.
- **Replacement Policies**: Supports **LRU** (Least Recently Used), **FIFO** (First-In, First-Out), and **Random** replacement policies.
  - Set-associative caches use hardware-like age counters.
  - Fully-associative tracking uses a fast $O(1)$ tombstone-based open-addressed hash map and static doubly-linked list for maximum simulation speed.
- **Write Policies**: Supports **Write-Back** and **Write-Through** policies.
- **Write Allocation**: Toggle write-allocate options (`yes` / `no`).
- **Performance Evaluation**:
  - Total cache accesses, read accesses, and write accesses.
  - Overall cache hits and misses.
  - Miss categorization (using the 3C model):
    - **Compulsory (Cold) Misses**: Occur when a memory block is accessed for the first time.
    - **Conflict Misses**: Occur due to set mapping limitations (would be a hit in a fully associative cache).
    - **Capacity Misses**: Occur when the cache cannot hold all required blocks (would also miss in a fully associative cache).
  - Dirty block evictions count (for Write-Back policy).
  - Hit rate and miss rate percentages.
  - **Average Memory Access Time (AMAT)** calculation.

---

## File Structure

- [cache_sim.c](file:///home/apratim/cache_simulator/cache_sim.c) - Core C source code implementing the simulator logic.
- [config.txt](file:///home/apratim/cache_simulator/config.txt) - Configuration file defining cache parameters.
- [trace.txt](file:///home/apratim/cache_simulator/trace.txt) - Sample trace file containing memory read/write requests.
- [Makefile](file:///home/apratim/cache_simulator/Makefile) - Build system file to compile and run the simulator easily.
- [README.md](file:///home/apratim/cache_simulator/README.md) - Documentation.

---

## Configuration File (`config.txt`)

The simulator reads parameters from a key-value format configuration file. 

### Example Configuration:
```ini
cache_size = 32768            # Cache size in bytes (e.g., 32 KB)
block_size = 64               # Block size in bytes
associativity = 4             # 4-way set associative
replacement_policy = LRU      # LRU / FIFO / Random
write_policy = write_back     # write_back / write_through
write_allocate = yes          # yes / no
address_bits = 32             # Memory address width

hit_time = 2 ns               # Cache hit latency
miss_time = 20 ns             # Main memory miss latency / penalty
```

---

## Trace File (`trace.txt`)

The trace file lists memory operations line by line. Each line consists of the operation type (`R` for Read, `W` for Write) and the memory address in hexadecimal format.

### Example Trace:
```text
R 0x00000000
W 0x00000000
R 0x00002000
R 0x00004000
R 0x00006000
R 0x00008000
R 0x00000000
```

---

## Getting Started

### Prerequisites
You need a C compiler supporting C11 (like `gcc`) and `make` installed on your system.

### Compilation
To compile the cache simulator, run:
```bash
make
```
This compiles the source code with high optimizations (`-O3`) and generates the executable `cache_sim`.

### Running the Simulator
To run the simulator:
```bash
./cache_sim config.txt trace.txt
```
Or use the convenience make command:
```bash
make run
```

### Cleanup
To clean up the compiled binary:
```bash
make clean
```

---

## Output (`output.txt`)

The simulator writes detailed results to `output.txt`.

### Example Output:
```text
Cache Configuration:
Cache Size: 32 KB
Block Size: 64 B
Associativity: 4-way
Replacement: LRU
Write Policy: write_back
Write Allocate: yes

Results:
Total Accesses: 7
Reads: 6
Writes: 1

Hits: 1
Misses: 6
Dirty Evictions: 1

Compulsory Misses: 5
Conflict Misses: 1
Capacity Misses: 0

Hit Rate: 14.29%
Miss Rate: 85.71%

AMAT: 19.14 ns
```
