#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/*#define MURMUR_IMPLEMENTATION*/
/*#include "murmur3hash.h"*/

// values in nanoseconds
uint64_t timer() {
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}

	return (uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec;
}

static double duration_in_s(uint64_t start, uint64_t end) {
	return (double)(end - start) / 1.0e9;
}

typedef uint32_t testfunc_t(uint32_t niter);

const int niter = 1000000000; // 1 billion

void microbench(const char *name, testfunc_t *fn) {
	uint32_t h = 0;

	// warm up
	h = fn(niter / 4);
	printf("warmup: %08x\n", h);

	uint64_t startt = timer();
	h = fn(niter);
	uint64_t endt = timer();
	printf("result: %08x\n", h);
	double secs = duration_in_s(startt, endt);
	double clock_rate_ghz = 1.65;
	double ns_per_iter = (secs * 1e9) / (double)niter;
	double clocks_per_iter = ns_per_iter * clock_rate_ghz;

	printf("%s: %.2f ns/iter (=%.2f cycles @ %.2fGHz)\n", name, ns_per_iter, clocks_per_iter, clock_rate_ghz);
}

// ---------------------------------

#define _TILE_TYPES_COUNT 8 // must be 8 because tile type field is 3 bit only
const uint8_t tile_invalid = -1;

typedef struct {
	char name[16];
	uint32_t color;
	struct {
		uint8_t matter, mind;
	} walk_cost[3];
} _tile_type_t;
_tile_type_t _tile_types[_TILE_TYPES_COUNT];

uint32_t colors[_TILE_TYPES_COUNT];

uint32_t* bench_colors;

void init_tiles() {
	srand(time(0));
	for (size_t t = 0; t < _TILE_TYPES_COUNT; ++t) {
		colors[t] = rand() + t;
		_tile_types[t].color = colors[t];
		_tile_types[t].walk_cost[0].matter = t;
		_tile_types[t].walk_cost[0].mind = t * 128;
	}
}

uint8_t tile_type_from_color(uint32_t rgb) {
	uint8_t type = tile_invalid;
	for (uint8_t k = 0; k < _TILE_TYPES_COUNT; ++k) {
		if (_tile_types[k].color == rgb) {
			type = k;
			break;
		}
	}
	return type;
}

inline uint32_t _rotl32(uint32_t x, int8_t r) { return (x << r) | (x >> (32 - r)); }
#define _ROTL32(x,y) _rotl32(x, y)

static uint32_t MURMUR_SEED = 0x5f3759df;

uint32_t murmur4(uint32_t data) {
	uint32_t h1 = MURMUR_SEED;
	uint32_t k1 = data;
	k1 *= 0xcc9e2d51; // round
	k1  = _ROTL32(k1, 15);
	k1 *= 0x1b873593;
	h1 ^= k1;
	h1  = _ROTL32(h1, 13);
	h1  = h1 * 5 + 0xe6546b64;
	h1 ^= 4;        // len
	h1 ^= h1 >> 16; // fmix32
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;
	return h1;
}

uint8_t hashmap[_TILE_TYPES_COUNT];

void bruteforce_perfect_seed() {
	int collisions = 1;
	while (collisions != 0) {
		MURMUR_SEED++;
		printf("trying seed = %x\n", MURMUR_SEED);

		size_t indices[_TILE_TYPES_COUNT];

		for (size_t c = 0; c < _TILE_TYPES_COUNT; ++c) {
			uint32_t h = murmur4(colors[c]);
			size_t i = h & (_TILE_TYPES_COUNT - 1);
			indices[c] = i;
			hashmap[i] = tile_type_from_color(colors[c]);
		}

		collisions = 0;
		for (size_t i = 0; i < _TILE_TYPES_COUNT; ++i) {
			for (size_t j = 0; j < _TILE_TYPES_COUNT; ++j) {
				if (i == j) continue;

				if (indices[i] == indices[j]) {
					collisions++;
				}
			}
		}
		printf("collisions: %d\n", collisions);
	}

	printf("perfect seed = %x\n", MURMUR_SEED);

	for (size_t c = 0; c < _TILE_TYPES_COUNT; ++c) {
		uint32_t h = murmur4(colors[c]);
		size_t i = h & (_TILE_TYPES_COUNT - 1);
		printf("%d ", hashmap[i]);
	}
	printf("\n");
}

unsigned int randr(unsigned int min, unsigned int max) {
	double scaled = (double)rand()/RAND_MAX;
	return (max - min +1)*scaled + min;
}

uint32_t rand_color() {
	return colors[randr(0, _TILE_TYPES_COUNT)];
}

uint32_t linear_search(uint32_t niter) {
	uint32_t cost = 0;
	for (size_t i = 0; i < niter; ++i) {
		uint32_t c = bench_colors[i];
		uint32_t t = tile_type_from_color(c);
		cost += t;
	}
	return cost;
}

uint32_t hash(uint32_t niter) {
	uint32_t cost = 0;
	for (size_t i = 0; i < niter; ++i) {
		uint32_t h = murmur4(bench_colors[i]);
		size_t i = h & (_TILE_TYPES_COUNT - 1);
		uint32_t t = hashmap[i];
		cost += t;
	}
	return cost;
}

void init_data() {
	bench_colors = malloc(niter * sizeof(uint32_t));
	for (size_t i = 0; i < niter; ++i) {
		bench_colors[i] = rand_color();
	}
}

int main() {
	init_tiles();
	bruteforce_perfect_seed();
	init_data();

	microbench("linear-search", linear_search);
	microbench("hash-index", hash);

	free(bench_colors);

	return 0;
}
