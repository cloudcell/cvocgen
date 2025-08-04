#ifndef CVOCGEN_H
#define CVOCGEN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

// Hash table entry
typedef struct Ht_item {
    char* key;
    int count;
    struct Ht_item* next;
} Ht_item;

// Hash table
typedef struct HashTable {
    Ht_item** items;
    unsigned int size;       // Number of buckets
    unsigned int count;      // Number of items stored
    float load_threshold;    // Load factor threshold for resizing
} HashTable;

// Structure to hold a list of tokens
typedef struct {
    char** tokens;
    size_t count;
    size_t capacity;
} TokenList;

// Default values for hash table
#define HT_DEFAULT_SIZE 10000
#define HT_DEFAULT_LOAD_THRESHOLD 0.7

// Function prototypes for hash table
HashTable* ht_create(unsigned int size);
HashTable* ht_create_with_threshold(unsigned int size, float load_threshold);
void ht_free(HashTable* ht);
Ht_item* hash_search(HashTable* ht, const char* key);
void hash_insert_or_increment(HashTable* ht, const char* key);
int ht_resize(HashTable* ht, unsigned int new_size);

// Function prototypes for tokenization
TokenList* pre_tokenize(const char* text);
void free_token_list(TokenList* list);

// Function prototypes for vocabulary generation
HashTable* get_word_counts(const char* text);
void print_word_counts(HashTable* ht);
HashTable* get_pair_stats(TokenList* tokens);
// Find the best (most frequent) pair in the stats hash table
// Returns the best pair and sets *count to its frequency
const char* get_best_pair(HashTable* stats, int* count);
TokenList* merge_pair(TokenList* tokens, const char* pair);
HashTable* train_bpe(const char* text, int num_merges);
HashTable* train_bpe_from_file(const char* corpus_file, int num_merges);
// Vocabulary I/O functions are now in cvocgen_io.h

#endif // CVOCGEN_H
