#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_HEAP_SIZE 4000  // 1000 words * 4 bytes/word
#define MAX_HEAP_SIZE 400000    // 100,000 words * 4 bytes/word
#define WORD_SIZE 4             // 4 bytes for a 32-bit system

// Global variables
int use_first_fit = 1;  // 1 for first-fit, 0 for best-fit
int use_implicit_list = 1;  // 1 for implicit list, 0 for explicit list
size_t current_heap_size = INITIAL_HEAP_SIZE;

unsigned char heap[INITIAL_HEAP_SIZE];  // Simulated heap

typedef struct {
    unsigned int size : 29;
    unsigned int allocated : 1;
    unsigned int padding : 2;
} BlockHeader;

// Function declarations
void* myalloc(size_t size);
void* myrealloc(void* ptr, size_t size);
void myfree(void* ptr);
void mysbrk(int size);
void readInput(const char* filename);
void writeOutput(const char* filename);
void initializeHeap();

void initializeHeap() {
    BlockHeader* initial_header = (BlockHeader*)heap;
    initial_header->size = INITIAL_HEAP_SIZE;
    initial_header->allocated = 0;

    BlockHeader* initial_footer = (BlockHeader*)(heap + INITIAL_HEAP_SIZE - sizeof(BlockHeader));
    *initial_footer = *initial_header;
}

void* myalloc(size_t size) {
    size_t total_size = size + 2 * sizeof(BlockHeader);
    total_size += (total_size % 8) ? (8 - total_size % 8) : 0;

    for (int i = 0; i < current_heap_size;) {
        BlockHeader* header = (BlockHeader*)&heap[i];
        size_t block_size = header->size;

        if (!header->allocated && block_size >= total_size) {
            size_t remaining_size = block_size - total_size;
            if (remaining_size > sizeof(BlockHeader)) {
                header->size = total_size;
                BlockHeader* new_header = (BlockHeader*)&heap[i + total_size];
                new_header->size = remaining_size;
                new_header->allocated = 0;

                BlockHeader* new_footer = (BlockHeader*)((char*)new_header + remaining_size - sizeof(BlockHeader));
                *new_footer = *new_header;
            }

            header->allocated = 1;
            BlockHeader* footer = (BlockHeader*)((char*)header + header->size - sizeof(BlockHeader));
            footer->allocated = 1;
            footer->size = header->size;
            return (void*)(header + 1);
        }

        i += block_size;
    }

    return NULL;
}

void* myrealloc(void* ptr, size_t size) {
    if (!ptr) {
        return myalloc(size);
    }

    BlockHeader* header = (BlockHeader*)ptr - 1;
    size_t old_size = header->size - 2 * sizeof(BlockHeader);

    if (size == 0) {
        myfree(ptr);
        return NULL;
    }

    void* new_ptr = myalloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, (old_size < size) ? old_size : size);
        myfree(ptr);
    }

    return new_ptr;
}

void myfree(void* ptr) {
    if (!ptr) {
        return;
    }

    BlockHeader* header = (BlockHeader*)ptr - 1;
    header->allocated = 0;
    size_t block_size = header->size;

    BlockHeader* footer = (BlockHeader*)((char*)header + block_size - sizeof(BlockHeader));
    footer->allocated = 0;

    // Coalesce if next block is free
    BlockHeader* next_header = (BlockHeader*)((char*)footer + sizeof(BlockHeader));
    if ((char*)next_header < (char*)heap + current_heap_size && !next_header->allocated) {
        size_t next_block_size = next_header->size;
        block_size += next_block_size;
        header->size = block_size;

        BlockHeader* next_footer = (BlockHeader*)((char*)next_header + next_block_size - sizeof(BlockHeader));
        next_footer->size = block_size;
    }

    // Coalesce if previous block is free
    if ((char*)header > (char*)heap) {
        BlockHeader* prev_footer = (BlockHeader*)((char*)header - sizeof(BlockHeader));
        if (!prev_footer->allocated) {
            BlockHeader* prev_header = (BlockHeader*)((char*)header - prev_footer->size);
            size_t total_size = prev_header->size + block_size;
            prev_header->size = total_size;

            footer->size = total_size;
        }
    }
}

void mysbrk(int size) {
    size_t new_size = current_heap_size + size;
    if (new_size > MAX_HEAP_SIZE || new_size < INITIAL_HEAP_SIZE) {
        fprintf(stderr, "Error: Heap size adjustment out of bounds.\n");
        return;
    }

    current_heap_size = new_size;
    printf("Heap size adjusted to %zu bytes.\n", current_heap_size);
}

typedef struct {
    void* ptr;
    int ref;
} PointerRef;

PointerRef pointers[1000];

void readInput(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return;
    }

    char operation;
    int size, ref, new_ref;

    while (fscanf(file, " %c", &operation) != EOF) {
        switch (operation) {
            case 'a':
                if (fscanf(file, "%d, %d", &size, &ref) == 2) {
                    pointers[ref].ptr = myalloc(size);
                    pointers[ref].ref = ref;
                }
                break;
            case 'r':
                if (fscanf(file, "%d, %d, %d", &size, &ref, &new_ref) == 3) {
                    pointers[new_ref].ptr = myrealloc(pointers[ref].ptr, size);
                    pointers[new_ref].ref = new_ref;
                }
                break;
            case 'f':
                if (fscanf(file, "%d", &ref) == 1) {
                    myfree(pointers[ref].ptr);
                }
                break;
        }
    }

    fclose(file);
}

void writeOutput(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Error opening file");
        return;
    }

    for (int i = 0; i < current_heap_size; i += sizeof(BlockHeader)) {
        BlockHeader* header = (BlockHeader*)&heap[i];
        fprintf(file, "%d, 0x%08X\n", i, *(unsigned int*)header);
    }

    fclose(file);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input_file> <free_list_type> <allocation_strategy>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[2], "implicit") == 0) {
        use_implicit_list = 1;
    } else if (strcmp(argv[2], "explicit") == 0) {
        use_implicit_list = 0;
    } else {
        fprintf(stderr, "Invalid free list type. Use 'implicit' or 'explicit'.\n");
        return 1;
    }

    if (strcmp(argv[3], "first-fit") == 0) {
        use_first_fit = 1;
    } else if (strcmp(argv[3], "best-fit") == 0) {
        use_first_fit = 0;
    } else {
        fprintf(stderr, "Invalid allocation strategy. Use 'first-fit' or 'best-fit'.\n");
        return 1;
    }

    initializeHeap();
    readInput(argv[1]);
    writeOutput("output.txt");

    return 0;
}