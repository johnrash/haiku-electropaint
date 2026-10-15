/*
 * malloc() for ep.c: memory below 2 GB, so that the 32-bit "registers" of
 * the decompiled code can hold the addresses. Nothing is ever reused, which
 * also keeps ep.c's reads of freed memory harmless.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static char* sArena;
static size_t sUsed;
static const size_t kArenaSize = 64 * 1024 * 1024;


void*
lowmalloc(size_t size)
{
	if (sArena == NULL) {
		sArena = mmap(NULL, kArenaSize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		if (sArena == MAP_FAILED || (uintptr_t)sArena + kArenaSize
				> 0x80000000UL) {
			fprintf(stderr, "lowmalloc: no memory below 2 GB\n");
			abort();
		}
	}
	size = (size + 15) & ~(size_t)15;
	if (sUsed + size > kArenaSize) {
		fprintf(stderr, "lowmalloc: arena exhausted\n");
		abort();
	}
	void* pointer = sArena + sUsed;
	sUsed += size;
	return pointer;
}


void
lowfree(void* pointer)
{
	(void)pointer;
}


char*
lowstrdup(const char* string)
{
	size_t length = strlen(string);
	// Two terminating nulls: ep.c steps over the first one when the script
	// does not end in a newline.
	char* copy = lowmalloc(length + 2);
	memcpy(copy, string, length);
	copy[length] = '\0';
	copy[length + 1] = '\0';
	return copy;
}
