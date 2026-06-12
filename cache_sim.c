#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * SECTION 1: Enums, Structs, and Global Types
 * ========================================================================== */

typedef enum {
    LRU,
    FIFO,
    RANDOM,
    LFU,
    PLRU,
    MRU
} ReplacementPolicy;

typedef enum {
    WRITE_BACK,
    WRITE_THROUGH
} WritePolicy;

typedef struct {
    bool hit;
    bool dirty_eviction;
} AccessResult;

typedef struct {
    bool valid;
    bool dirty;
    uint64_t tag;
    uint32_t age;
    uint32_t frequency;
} CacheBlock;

typedef struct {
    CacheBlock* blocks;
    uint8_t* plru_bits;
} CacheSet;

typedef struct {
    uint32_t num_sets;
    uint32_t associativity;
    uint32_t block_size;
    ReplacementPolicy replacement_policy;
    WritePolicy write_policy;
    bool write_allocate;

    bool use_bitwise;
    uint32_t offset_bits;
    uint32_t index_bits;
    uint64_t index_mask;

    CacheSet* sets;
} Cache;

typedef struct {
    uint64_t* tags;
    uint32_t size;
    uint32_t capacity;
} SeenTracker;

typedef struct {
    uint64_t cache_size;
    uint32_t block_size;
    uint32_t associativity;
    ReplacementPolicy replacement_policy;
    char replacement_policy_str[100];
    WritePolicy write_policy;
    char write_policy_str[100];
    bool write_allocate;
    uint32_t address_bits;
    double hit_time;
    char hit_time_unit[10];
    double miss_time;
    char miss_time_unit[10];
} Config;


/* ==========================================================================
 * SECTION 2: Function Prototypes
 * ========================================================================== */

uint32_t random_way(uint32_t assoc);
void age_update_on_hit(CacheBlock* blocks, uint32_t assoc, uint32_t hit_way);
void age_update_on_miss(CacheBlock* blocks, uint32_t assoc, uint32_t alloc_way);
void plru_update(uint8_t* plru_bits, uint32_t assoc, uint32_t way_idx);
uint32_t plru_select_victim(const uint8_t* plru_bits, uint32_t assoc);

void seen_init(SeenTracker* s);
void seen_free(SeenTracker* s);
bool seen_check_and_add(SeenTracker* s, uint64_t tag);

void cache_init(Cache* c, uint32_t sets_count, uint32_t assoc, uint32_t blk_sz,
                ReplacementPolicy rep_pol, WritePolicy wr_pol, bool wr_alloc);
void cache_free(Cache* c);
AccessResult cache_access(Cache* c, uint64_t address, bool is_write);

char* trim(char* str);
Config parse_config(const char* filepath);
void format_size(char* dest, uint64_t bytes);


/* ==========================================================================
 * SECTION 3: Main Orchestrator 
 * ========================================================================== */

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: ./cache_sim config.txt trace.txt\n");
        return 1;
    }

    Config config = parse_config(argv[1]);

    uint64_t num_blocks = config.cache_size / config.block_size;
    uint32_t num_sets = (uint32_t)(num_blocks / config.associativity);

    Cache main_cache;
    cache_init(&main_cache, num_sets, config.associativity, config.block_size,
               config.replacement_policy, config.write_policy, config.write_allocate);

    Cache fa_cache;
    cache_init(&fa_cache, 1, (uint32_t)num_blocks, config.block_size,
               config.replacement_policy, config.write_policy, config.write_allocate);

    FILE* trace_file = fopen(argv[2], "r");
    if (!trace_file) {
        fprintf(stderr, "Error: Could not open trace file: %s\n", argv[2]);
        cache_free(&main_cache);
        cache_free(&fa_cache);
        return 1;
    }

    uint64_t total_accesses = 0;
    uint64_t reads = 0;
    uint64_t writes = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t dirty_evictions = 0;

    uint64_t compulsory_misses = 0;
    uint64_t conflict_misses = 0;
    uint64_t capacity_misses = 0;

    SeenTracker seen_blocks;
    seen_init(&seen_blocks);

    char line[256];
    uint64_t addr_mask = (config.address_bits >= 64) ? ~0ULL : (1ULL << config.address_bits) - 1;

    while (fgets(line, sizeof(line), trace_file)) {
        char* trimmed = trim(line);
        if (strlen(trimmed) == 0) continue;

        char op_str[50] = "";
        char addr_str[100] = "";
        if (sscanf(trimmed, "%s %s", op_str, addr_str) != 2) continue;

        char op = toupper((unsigned char)op_str[0]);
        if (op != 'R' && op != 'W') continue;

        bool is_write = (op == 'W');
        uint64_t address = strtoull(addr_str, NULL, 16);
        address &= addr_mask;

        total_accesses++;
        if (is_write) {
            writes++;
        } else {
            reads++;
        }

        uint64_t block_addr = address / config.block_size;

        AccessResult main_res = cache_access(&main_cache, address, is_write);
        AccessResult fa_res = cache_access(&fa_cache, address, is_write);

        if (main_res.hit) {
            hits++;
        } else {
            misses++;
            if (main_res.dirty_eviction) {
                dirty_evictions++;
            }

            if (!seen_check_and_add(&seen_blocks, block_addr)) {
                compulsory_misses++;
            } else {
                if (fa_res.hit) {
                    conflict_misses++;
                } else {
                    capacity_misses++;
                }
            }
        }
    }

    fclose(trace_file);

    double hit_rate = 0.0;
    double miss_rate = 0.0;
    double amat = config.hit_time;
    if (total_accesses > 0) {
        hit_rate = (double)hits / total_accesses * 100.0;
        miss_rate = (double)misses / total_accesses * 100.0;
        amat = config.hit_time + ((double)misses / total_accesses) * config.miss_time;
    }

    FILE* out_file = fopen("output.txt", "w");
    if (!out_file) {
        fprintf(stderr, "Error: Could not open output.txt for writing\n");
        cache_free(&main_cache);
        cache_free(&fa_cache);
        seen_free(&seen_blocks);
        return 1;
    }

    char cache_sz_str[50];
    char block_sz_str[50];
    format_size(cache_sz_str, config.cache_size);
    format_size(block_sz_str, config.block_size);

    fprintf(out_file, "Cache Configuration:\n");
    fprintf(out_file, "Cache Size: %s\n", cache_sz_str);
    fprintf(out_file, "Block Size: %s\n", block_sz_str);
    fprintf(out_file, "Associativity: %d-way\n", config.associativity);
    fprintf(out_file, "Replacement: %s\n", config.replacement_policy_str);
    fprintf(out_file, "Write Policy: %s\n", config.write_policy_str);
    fprintf(out_file, "Write Allocate: %s\n\n", config.write_allocate ? "yes" : "no");

    fprintf(out_file, "Results:\n");
    fprintf(out_file, "Total Accesses: %llu\n", (unsigned long long)total_accesses);
    fprintf(out_file, "Reads: %llu\n", (unsigned long long)reads);
    fprintf(out_file, "Writes: %llu\n\n", (unsigned long long)writes);

    fprintf(out_file, "Hits: %llu\n", (unsigned long long)hits);
    fprintf(out_file, "Misses: %llu\n", (unsigned long long)misses);
    fprintf(out_file, "Dirty Evictions: %llu\n\n", (unsigned long long)dirty_evictions);

    fprintf(out_file, "Compulsory Misses: %llu\n", (unsigned long long)compulsory_misses);
    fprintf(out_file, "Conflict Misses: %llu\n", (unsigned long long)conflict_misses);
    fprintf(out_file, "Capacity Misses: %llu\n\n", (unsigned long long)capacity_misses);

    fprintf(out_file, "Hit Rate: %.2f%%\n", hit_rate);
    fprintf(out_file, "Miss Rate: %.2f%%\n\n", miss_rate);
    fprintf(out_file, "AMAT: %.2f %s\n", amat, config.hit_time_unit);

    fclose(out_file);

    cache_free(&main_cache);
    cache_free(&fa_cache);
    seen_free(&seen_blocks);

    return 0;
}


/* ==========================================================================
 * SECTION 4: Helper & Core Simulator Function Definitions (Bottom of File)
 * ========================================================================== */

uint32_t random_way(uint32_t assoc) {
    static uint32_t seed = 123456789;
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % assoc;
}

// In-place update of valid block ages on hit
void age_update_on_hit(CacheBlock* blocks, uint32_t assoc, uint32_t hit_way) {
    uint32_t hit_age = blocks[hit_way].age;
    for (uint32_t i = 0; i < assoc; ++i) {
        if (i != hit_way && blocks[i].valid && blocks[i].age < hit_age) {
            blocks[i].age++;
        }
    }
    blocks[hit_way].age = 0;
}

// In-place update of valid block ages on miss
void age_update_on_miss(CacheBlock* blocks, uint32_t assoc, uint32_t alloc_way) {
    for (uint32_t i = 0; i < assoc; ++i) {
        if (i != alloc_way && blocks[i].valid) {
            blocks[i].age++;
        }
    }
    blocks[alloc_way].age = 0;
}

void plru_update(uint8_t* plru_bits, uint32_t assoc, uint32_t way_idx) {
    if (assoc <= 1) return;
    uint32_t n = 0;
    uint32_t temp = assoc;
    uint32_t D = 0;
    while (temp >>= 1) D++;

    for (int d = (int)D - 1; d >= 0; --d) {
        uint32_t turn = (way_idx >> d) & 1;
        if (turn == 0) {
            plru_bits[n] = 1;
            n = 2 * n + 1;
        } else {
            plru_bits[n] = 0;
            n = 2 * n + 2;
        }
    }
}

uint32_t plru_select_victim(const uint8_t* plru_bits, uint32_t assoc) {
    if (assoc <= 1) return 0;
    uint32_t n = 0;
    uint32_t victim = 0;
    uint32_t temp = assoc;
    uint32_t D = 0;
    while (temp >>= 1) D++;

    for (uint32_t d = 0; d < D; ++d) {
        uint32_t bit = plru_bits[n];
        if (bit == 0) {
            victim = (victim << 1) | 0;
            n = 2 * n + 1;
        } else {
            victim = (victim << 1) | 1;
            n = 2 * n + 2;
        }
    }
    return victim;
}

void seen_init(SeenTracker* s) {
    s->size = 0;
    s->capacity = 1024;
    s->tags = malloc(s->capacity * sizeof(uint64_t));
}

void seen_free(SeenTracker* s) {
    free(s->tags);
}

bool seen_check_and_add(SeenTracker* s, uint64_t tag) {
    for (uint32_t i = 0; i < s->size; ++i) {
        if (s->tags[i] == tag) {
            return true;
        }
    }
    if (s->size >= s->capacity) {
        s->capacity *= 2;
        s->tags = realloc(s->tags, s->capacity * sizeof(uint64_t));
    }
    s->tags[s->size++] = tag;
    return false;
}

void cache_init(Cache* c, uint32_t sets_count, uint32_t assoc, uint32_t blk_sz,
                ReplacementPolicy rep_pol, WritePolicy wr_pol, bool wr_alloc) {
    c->num_sets = sets_count;
    c->associativity = assoc;
    c->block_size = blk_sz;
    c->replacement_policy = rep_pol;
    c->write_policy = wr_pol;
    c->write_allocate = wr_alloc;

    c->sets = malloc(sets_count * sizeof(CacheSet));
    for (uint32_t i = 0; i < sets_count; ++i) {
        c->sets[i].blocks = calloc(assoc, sizeof(CacheBlock));
        if (assoc > 1) {
            c->sets[i].plru_bits = calloc(assoc - 1, sizeof(uint8_t));
        } else {
            c->sets[i].plru_bits = NULL;
        }
    }

    c->use_bitwise = (blk_sz && !(blk_sz & (blk_sz - 1))) &&
                     (sets_count && !(sets_count & (sets_count - 1)));
    if (c->use_bitwise) {
        c->offset_bits = 0;
        uint32_t temp = blk_sz;
        while (temp >>= 1) c->offset_bits++;

        c->index_bits = 0;
        temp = sets_count;
        while (temp >>= 1) c->index_bits++;

        c->index_mask = sets_count - 1;
    }
}

void cache_free(Cache* c) {
    for (uint32_t i = 0; i < c->num_sets; ++i) {
        free(c->sets[i].blocks);
        if (c->sets[i].plru_bits) {
            free(c->sets[i].plru_bits);
        }
    }
    free(c->sets);
}

AccessResult cache_access(Cache* c, uint64_t address, bool is_write) {
    uint64_t block_addr = address / c->block_size;
    uint32_t set_idx = 0;
    uint64_t tag = 0;

    if (c->use_bitwise) {
        set_idx = (uint32_t)(block_addr & c->index_mask);
        tag = block_addr >> c->index_bits;
    } else {
        set_idx = (uint32_t)(block_addr % c->num_sets);
        tag = block_addr / c->num_sets;
    }

    CacheSet* set = &c->sets[set_idx];

    int hit_way = -1;
    for (uint32_t i = 0; i < c->associativity; ++i) {
        if (set->blocks[i].valid && set->blocks[i].tag == tag) {
            hit_way = (int)i;
            break;
        }
    }

    AccessResult res;
    res.hit = false;
    res.dirty_eviction = false;

    if (hit_way != -1) {
        res.hit = true;
        if (is_write && c->write_policy == WRITE_BACK) {
            set->blocks[hit_way].dirty = true;
        }

        if (c->replacement_policy == LFU) {
            set->blocks[hit_way].frequency++;
        }

        if (c->replacement_policy == LRU || c->replacement_policy == MRU) {
            age_update_on_hit(set->blocks, c->associativity, (uint32_t)hit_way);
        } else if (c->replacement_policy == PLRU) {
            plru_update(set->plru_bits, c->associativity, (uint32_t)hit_way);
        }
    } else {
        if (!is_write || c->write_allocate) {
            int alloc_way = -1;
            for (uint32_t i = 0; i < c->associativity; ++i) {
                if (!set->blocks[i].valid) {
                    alloc_way = (int)i;
                    break;
                }
            }

            if (alloc_way != -1) {
                set->blocks[alloc_way].valid = true;
                set->blocks[alloc_way].tag = tag;
                set->blocks[alloc_way].dirty = (is_write && c->write_policy == WRITE_BACK);
                set->blocks[alloc_way].frequency = 1;

                if (c->replacement_policy == LRU || c->replacement_policy == FIFO || c->replacement_policy == MRU) {
                    age_update_on_miss(set->blocks, c->associativity, (uint32_t)alloc_way);
                } else if (c->replacement_policy == PLRU) {
                    plru_update(set->plru_bits, c->associativity, (uint32_t)alloc_way);
                }
            } else {
                uint32_t victim_way = 0;
                if (c->replacement_policy == RANDOM) {
                    victim_way = random_way(c->associativity);
                } else if (c->replacement_policy == PLRU) {
                    victim_way = plru_select_victim(set->plru_bits, c->associativity);
                } else if (c->replacement_policy == MRU) {
                    for (uint32_t i = 0; i < c->associativity; ++i) {
                        if (set->blocks[i].age == 0) {
                            victim_way = i;
                            break;
                        }
                    }
                } else if (c->replacement_policy == LFU) {
                    uint32_t min_freq = set->blocks[0].frequency;
                    uint32_t max_age = set->blocks[0].age;
                    victim_way = 0;
                    for (uint32_t i = 1; i < c->associativity; ++i) {
                        if (set->blocks[i].frequency < min_freq) {
                            min_freq = set->blocks[i].frequency;
                            max_age = set->blocks[i].age;
                            victim_way = i;
                        } else if (set->blocks[i].frequency == min_freq) {
                            if (set->blocks[i].age > max_age) {
                                max_age = set->blocks[i].age;
                                victim_way = i;
                            }
                        }
                    }
                } else {
                    uint32_t max_age = 0;
                    for (uint32_t i = 0; i < c->associativity; ++i) {
                        if (set->blocks[i].age > max_age) {
                            max_age = set->blocks[i].age;
                            victim_way = i;
                        }
                    }
                }

                if (set->blocks[victim_way].dirty && c->write_policy == WRITE_BACK) {
                    res.dirty_eviction = true;
                }

                set->blocks[victim_way].tag = tag;
                set->blocks[victim_way].dirty = (is_write && c->write_policy == WRITE_BACK);
                set->blocks[victim_way].frequency = 1;

                if (c->replacement_policy == LRU || c->replacement_policy == FIFO || c->replacement_policy == MRU) {
                    age_update_on_miss(set->blocks, c->associativity, victim_way);
                } else if (c->replacement_policy == PLRU) {
                    plru_update(set->plru_bits, c->associativity, victim_way);
                }
            }
        }
    }

    return res;
}

char* trim(char* str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

Config parse_config(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open configuration file: %s\n", filepath);
        exit(1);
    }

    Config config;
    memset(&config, 0, sizeof(Config));
    config.address_bits = 32;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* comment = strchr(line, '#');
        if (comment) {
            *comment = '\0';
        }

        char* trimmed = trim(line);
        if (strlen(trimmed) == 0) continue;

        char* eq = strchr(trimmed, '=');
        if (!eq) {
            fprintf(stderr, "Error: Invalid line in config: %s\n", trimmed);
            exit(1);
        }

        *eq = '\0';
        char* key = trim(trimmed);
        char* val = trim(eq + 1);

        if (strcmp(key, "cache_size") == 0) {
            config.cache_size = strtoull(val, NULL, 10);
        } else if (strcmp(key, "block_size") == 0) {
            config.block_size = (uint32_t)strtoul(val, NULL, 10);
        } else if (strcmp(key, "associativity") == 0) {
            config.associativity = (uint32_t)strtoul(val, NULL, 10);
        } else if (strcmp(key, "replacement_policy") == 0) {
            strcpy(config.replacement_policy_str, val);
            if (strcmp(val, "LRU") == 0) {
                config.replacement_policy = LRU;
            } else if (strcmp(val, "FIFO") == 0) {
                config.replacement_policy = FIFO;
            } else if (strcmp(val, "Random") == 0) {
                config.replacement_policy = RANDOM;
            } else if (strcmp(val, "LFU") == 0) {
                config.replacement_policy = LFU;
            } else if (strcmp(val, "PLRU") == 0) {
                config.replacement_policy = PLRU;
            } else if (strcmp(val, "MRU") == 0) {
                config.replacement_policy = MRU;
            } else {
                fprintf(stderr, "Error: Unknown replacement policy: %s\n", val);
                exit(1);
            }
        } else if (strcmp(key, "write_policy") == 0) {
            strcpy(config.write_policy_str, val);
            if (strcmp(val, "write_back") == 0) {
                config.write_policy = WRITE_BACK;
            } else if (strcmp(val, "write_through") == 0) {
                config.write_policy = WRITE_THROUGH;
            } else {
                fprintf(stderr, "Error: Unknown write policy: %s\n", val);
                exit(1);
            }
        } else if (strcmp(key, "write_allocate") == 0) {
            if (strcmp(val, "yes") == 0) {
                config.write_allocate = true;
            } else if (strcmp(val, "no") == 0) {
                config.write_allocate = false;
            } else {
                fprintf(stderr, "Error: Invalid write_allocate option: %s\n", val);
                exit(1);
            }
        } else if (strcmp(key, "address_bits") == 0) {
            config.address_bits = (uint32_t)strtoul(val, NULL, 10);
        } else if (strcmp(key, "hit_time") == 0) {
            sscanf(val, "%lf %s", &config.hit_time, config.hit_time_unit);
        } else if (strcmp(key, "miss_time") == 0) {
            sscanf(val, "%lf %s", &config.miss_time, config.miss_time_unit);
        }
    }

    fclose(file);

    if (config.cache_size == 0 || config.block_size == 0 || config.associativity == 0) {
        fprintf(stderr, "Error: Missing essential cache parameters in config\n");
        exit(1);
    }
    if (config.cache_size % config.block_size != 0) {
        fprintf(stderr, "Error: Cache size must be divisible by block size\n");
        exit(1);
    }
    uint64_t num_blocks = config.cache_size / config.block_size;
    if (num_blocks % config.associativity != 0) {
        fprintf(stderr, "Error: Cache blocks must be divisible by associativity\n");
        exit(1);
    }

    return config;
}

void format_size(char* dest, uint64_t bytes) {
    if (bytes >= 1024 * 1024) {
        sprintf(dest, "%llu MB", (unsigned long long)(bytes / (1024 * 1024)));
    } else if (bytes >= 1024) {
        sprintf(dest, "%llu KB", (unsigned long long)(bytes / 1024));
    } else {
        sprintf(dest, "%llu B", (unsigned long long)bytes);
    }
}
