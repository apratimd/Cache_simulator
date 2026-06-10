# C Cache Simulator

A highly configurable Cache Simulator written in C. It simulates a cache memory architecture based on custom parameters, processes memory access traces, and evaluates performance metrics like hit/miss rates, Average Memory Access Time (AMAT), and types of cache misses (Compulsory, Capacity, and Conflict).

---

## Features

- **Configurable Architecture**: Easily configure cache size, block size, and associativity.
- **Replacement Policies**: Supports **LRU** (Least Recently Used) and **Random** replacement policies.
- **Write Policies**: Supports **Write-Back** and **Write-Through** policies.
- **Write Allocation**: Toggle write-allocate options (`yes` / `no`).
- **Performance Evaluation**:
  - Total cache accesses, read accesses, and write accesses.
  - Overall cache hits and misses.
  - Miss categorization:
    - **Compulsory (Cold) Misses**: Occur when a memory block is accessed for the first time.
    - **Conflict Misses**: Occur due to set mapping limitations (would be a hit in a fully associative cache).
    - **Capacity Misses**: Occur when the cache cannot hold all required blocks (would also miss in a fully associative cache).
  - Dirty block evictions count (for Write-Back policy).
  - Hit rate and miss rate percentages.
  - **Average Memory Access Time (AMAT)** calculation.

---

## File Structure

- `cache_sim.c` - Core C source code implementing the simulator logic.
- `Makefile` - Facilitates compilation, execution, and cleanup.
- `config.txt` - Configuration file defining cache parameters.
- `trace.txt` - Sample trace file containing memory read/write requests.
- `README.md` - Documentation.

---

## Configuration File (`config.txt`)

The simulator reads parameters from a key-value format configuration file. 

### Example Configuration:
```ini
cache_size = 32768            # Cache size in bytes (e.g., 32 KB)
block_size = 64               # Block size in bytes
associativity = 4             # 4-way set associative
replacement_policy = LRU      # LRU / Random
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
You need a C compiler (like `gcc`) and `make` installed on your system.

### Compilation
To compile the cache simulator, run:
```bash
make
```
This compiles the source code and generates the executable `cache_sim`.

### Running the Simulator
To run the simulator with your configuration and trace files:
```bash
./cache_sim config.txt trace.txt
```
Alternatively, you can use the Makefile shortcut to run with default files:
```bash
make run
```

### Cleanup
To clean up compiled binaries and output files:
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

Hits: 2
Misses: 5
Dirty Evictions: 0

Compulsory Misses: 5
Conflict Misses: 0
Capacity Misses: 0

Hit Rate: 28.57%
Miss Rate: 71.43%

AMAT: 16.29 ns
```
