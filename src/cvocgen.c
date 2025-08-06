#include "cvocgen.h"
#include "cvocgen_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>  /* For PATH_MAX */
#include "progress_bar.h"

// Global variables
int input_format_is_smiles = 0; // 0 = SELFIES (default), 1 = SMILES
char output_directory[PATH_MAX] = "."; // Default to current directory

// Hash function
unsigned int hash(const char* key, unsigned int size) {
    unsigned long int value = 0;
    unsigned int i = 0;
    unsigned int key_len = strlen(key);

    for (; i < key_len; ++i) {
        value = value * 37 + key[i];
    }

    return value % size;
}

// Create a hash table with default load threshold
HashTable* ht_create(unsigned int size) {
    return ht_create_with_threshold(size, HT_DEFAULT_LOAD_THRESHOLD);
}

// Create a hash table with specified size and load threshold
HashTable* ht_create_with_threshold(unsigned int size, float load_threshold) {
    // Use default size if 0 is provided
    if (size == 0) {
        size = HT_DEFAULT_SIZE;
    }
    
    HashTable* ht = malloc(sizeof(HashTable));
    if (!ht) return NULL;

    ht->size = size;
    ht->count = 0;
    ht->load_threshold = load_threshold;
    ht->items = calloc(ht->size, sizeof(Ht_item*));
    if (!ht->items) {
        free(ht);
        return NULL;
    }
    return ht;
}

// Free a hash table
void ht_free(HashTable* ht) {
    if (!ht) return;

    for (unsigned int i = 0; i < ht->size; ++i) {
        Ht_item* item = ht->items[i];
        while (item) {
            Ht_item* next = item->next;
            free(item->key);
            free(item);
            item = next;
        }
    }

    free(ht->items);
    free(ht);
}

// Search in hash table for an item
Ht_item* hash_search(HashTable* ht, const char* key) {
    unsigned int slot = hash(key, ht->size);
    Ht_item* item = ht->items[slot];

    while (item) {
        if (strcmp(item->key, key) == 0) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

// Resize the hash table to a new size
int ht_resize(HashTable* ht, unsigned int new_size) {
    if (!ht || new_size == 0) return 0;
    
    // Create a new items array
    Ht_item** new_items = calloc(new_size, sizeof(Ht_item*));
    if (!new_items) return 0;
    
    // Rehash all existing items
    for (unsigned int i = 0; i < ht->size; ++i) {
        Ht_item* item = ht->items[i];
        while (item) {
            // Get next item before modifying current item
            Ht_item* next = item->next;
            
            // Insert item into new array
            unsigned int new_slot = hash(item->key, new_size);
            item->next = new_items[new_slot];
            new_items[new_slot] = item;
            
            // Move to next item
            item = next;
        }
    }
    
    // Free old array and update hash table
    free(ht->items);
    ht->items = new_items;
    ht->size = new_size;
    
    return 1;
}

// Insert into hash table or increment count
void hash_insert_or_increment(HashTable* ht, const char* key) {
    // Check if we need to resize before inserting
    float load_factor = (float)(ht->count + 1) / ht->size;
    if (load_factor >= ht->load_threshold) {
        // Double the size when resizing
        ht_resize(ht, ht->size * 2);
    }
    
    Ht_item* item = hash_search(ht, key);

    if (item) {
        // Found, increment count
        item->count++;
    } else {
        // Not found, insert new item with count 1
        unsigned int slot = hash(key, ht->size);
        Ht_item* new_item = malloc(sizeof(Ht_item));
        new_item->key = strdup(key);
        new_item->count = 1;
        new_item->next = ht->items[slot];
        ht->items[slot] = new_item;
        
        // Increment item count
        ht->count++;
    }
}

// Pre-tokenize a string using a regex pattern based on input format
TokenList* pre_tokenize(const char* text) {
    const char* smiles_pattern = "(\\[[^]]+\\]|Br?|Cl?|N|O|S|P|F|I|b|c|n|o|s|p|\\(|\\)|\\.|=|#|-|\\+|\\\\|/|:|~|@|\\?|>|\\*|\\$|%[0-9]{2}|[0-9])";
    const char* selfies_pattern = "(\\[[^]]+\\]|\\.)";
    
    // Choose pattern based on global format setting
    const char* pattern = input_format_is_smiles ? smiles_pattern : selfies_pattern;

    regex_t re;
    int reti;

    reti = regcomp(&re, pattern, REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        return NULL;
    }

    TokenList* list = malloc(sizeof(TokenList));
    list->tokens = malloc(sizeof(char*) * 10); // Initial capacity
    list->count = 0;
    list->capacity = 10;

    const char* p = text;
    regmatch_t pmatch[1];

    while (1) {
        if (regexec(&re, p, 1, pmatch, 0)) {
            break; // No more matches
        }

        int len = pmatch[0].rm_eo - pmatch[0].rm_so;
        if (list->count >= list->capacity) {
            list->capacity *= 2;
            list->tokens = realloc(list->tokens, sizeof(char*) * list->capacity);
        }

        char* token = malloc(len + 1);
        strncpy(token, p + pmatch[0].rm_so, len);
        token[len] = '\0';
        list->tokens[list->count++] = token;

        p += pmatch[0].rm_eo;
    }

    regfree(&re);
    return list;
}

void free_token_list(TokenList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        free(list->tokens[i]);
    }
    free(list->tokens);
    free(list);
}

// Get word counts from a text
HashTable* get_word_counts(const char* text) {
    TokenList* tokens = pre_tokenize(text);
    if (!tokens) {
        return NULL;
    }

    HashTable* ht = ht_create(100); // Create a hash table with a reasonable size

    for (size_t i = 0; i < tokens->count; ++i) {
        hash_insert_or_increment(ht, tokens->tokens[i]);
    }

    free_token_list(tokens);
    return ht;
}

unsigned int count_unique_tokens(HashTable* ht) {
    unsigned int count = 0;
    for (unsigned int i = 0; i < ht->size; ++i) {
        Ht_item* item = ht->items[i];
        while (item) {
            count++;
            item = item->next;
        }
    }
    return count;
}

void print_word_counts(HashTable* ht) {
    if (!ht) return;
    printf("Word Counts:\n");
    printf("Unique tokens: %u\n", count_unique_tokens(ht));
    for (unsigned int i = 0; i < ht->size; ++i) {
        Ht_item* item = ht->items[i];
        while (item) {
            printf("  - '%s': %d\n", item->key, item->count);
            item = item->next;
        }
    }
}

// Get statistics on adjacent pairs
HashTable* get_pair_stats(TokenList* tokens) {
    if (!tokens || tokens->count < 2) {
        return NULL;
    }

    HashTable* stats = ht_create(HT_DEFAULT_SIZE / 10); // Create a new hash table for pair stats

    for (size_t i = 0; i < tokens->count - 1; ++i) {
        // Create a key for the pair, e.g., "[C] [C]"
        char pair_key[256];
        snprintf(pair_key, sizeof(pair_key), "%s %s", tokens->tokens[i], tokens->tokens[i+1]);
        hash_insert_or_increment(stats, pair_key);
    }

    return stats;
}

// Find the best (most frequent) pair in the stats hash table
// Returns the best pair and sets *count to its frequency
const char* get_best_pair(HashTable* stats, int* count) {
    if (!stats || !count) {
        return NULL;
    }

    const char* best_pair = NULL;
    int max_count = -1;

    for (unsigned int i = 0; i < stats->size; ++i) {
        Ht_item* item = stats->items[i];
        while (item) {
            if (item->count > max_count) {
                max_count = item->count;
                best_pair = item->key;
            }
            item = item->next;
        }
    }
    
    *count = max_count;
    return best_pair;
}

// Merge adjacent tokens that match the specified pair
TokenList* merge_pair(TokenList* tokens, const char* pair) {
    if (!tokens || !pair) {
        return tokens;
    }

    // Split the pair into first and second tokens
    char* pair_copy = strdup(pair);
    char* first = pair_copy;
    char* second = NULL;
    
    // Find the space separating the two tokens
    char* space = strchr(pair_copy, ' ');
    if (space) {
        *space = '\0';  // Split the string
        second = space + 1;
    } else {
        // Invalid pair format
        free(pair_copy);
        return tokens;
    }

    // Create a new token list for the result
    TokenList* result = malloc(sizeof(TokenList));
    result->capacity = tokens->capacity;
    result->count = 0;
    result->tokens = malloc(sizeof(char*) * result->capacity);

    // Process the tokens
    for (size_t i = 0; i < tokens->count; ++i) {
        // Check if this token and the next one form the pair
        if (i < tokens->count - 1 && 
            strcmp(tokens->tokens[i], first) == 0 && 
            strcmp(tokens->tokens[i+1], second) == 0) {
            
            // Create the merged token
            char merged[256];
            snprintf(merged, sizeof(merged), "%s%s", first, second);
            
            // Add the merged token to the result
            if (result->count >= result->capacity) {
                result->capacity *= 2;
                result->tokens = realloc(result->tokens, sizeof(char*) * result->capacity);
            }
            result->tokens[result->count++] = strdup(merged);
            
            // Skip the next token since we've merged it
            i++;
        } else {
            // Just copy the current token
            if (result->count >= result->capacity) {
                result->capacity *= 2;
                result->tokens = realloc(result->tokens, sizeof(char*) * result->capacity);
            }
            result->tokens[result->count++] = strdup(tokens->tokens[i]);
        }
    }

    free(pair_copy);
    return result;
}

// Save vocabulary and merges to a file
// These functions have been moved to cvocgen_io.h

// This function has been moved to cvocgen_io.h

// Train BPE on the given text for a specified number of merges
HashTable* train_bpe(const char* text, int num_merges) {
    if (!text || num_merges <= 0) {
        return NULL;
    }

    // 1. Initial tokenization
    TokenList* tokens = pre_tokenize(text);
    if (!tokens) {
        return NULL;
    }

    // 2. Create initial vocabulary from tokens
    HashTable* vocab = ht_create(100);
    for (size_t i = 0; i < tokens->count; ++i) {
        hash_insert_or_increment(vocab, tokens->tokens[i]);
    }

    // 3. Perform BPE merges
    char** merges = malloc(sizeof(char*) * num_merges);
    int merge_count = 0;

    for (int i = 0; i < num_merges; ++i) {
        // Get pair statistics
        HashTable* pair_stats = get_pair_stats(tokens);
        if (!pair_stats) {
            break;
        }

        // Find the best pair
        int pair_count = 0;
        const char* best_pair = get_best_pair(pair_stats, &pair_count);
        if (!best_pair) {
            ht_free(pair_stats);
            break;
        }

        // Store the merge operation
        merges[merge_count++] = strdup(best_pair);

        // Merge the best pair
        TokenList* new_tokens = merge_pair(tokens, best_pair);
        if (!new_tokens) {
            ht_free(pair_stats);
            break;
        }

        // Update the vocabulary with the merged token
        // Split the pair into first and second tokens
        char* pair_copy = strdup(best_pair);
        char* first = pair_copy;
        char* second = NULL;
        
        // Find the space separating the two tokens
        char* space = strchr(pair_copy, ' ');
        if (space) {
            *space = '\0';  // Split the string
            second = space + 1;
            
            // Create the merged token
            char merged[256];
            snprintf(merged, sizeof(merged), "%s%s", first, second);
            
            // Add to vocabulary
            hash_insert_or_increment(vocab, merged);
        }
        free(pair_copy);

        // Free old tokens and update
        free_token_list(tokens);
        tokens = new_tokens;

        ht_free(pair_stats);
    }

    // Save the vocabulary and merges in both formats
    save_vocabulary(vocab, merges, merge_count, "vocab.txt");
    save_vocabulary_json(vocab, merges, merge_count, "vocab");

    // Free the merge operations array
    for (int i = 0; i < merge_count; ++i) {
        free(merges[i]);
    }
    free(merges);

    // Free the final token list
    free_token_list(tokens);

    return vocab;
}

// Train BPE on a corpus from a file
HashTable* train_bpe_from_file(const char* corpus_file, int num_merges) {
    if (!corpus_file || num_merges < 0) {
        return NULL;
    }

    // Open the corpus file
    FILE* file = fopen(corpus_file, "r");
    if (!file) {
        perror("Error opening corpus file");
        return NULL;
    }

    // Create a hash table for the initial vocabulary
    HashTable* vocab = ht_create(HT_DEFAULT_SIZE);  // Larger initial size for corpus
    
    // Debug: Print file info
    printf("Processing file: %s\n", corpus_file);

    // Read the file line by line and process each molecule
    char line[65536];  // Increased buffer size for very long lines
    int line_count = 0;
    
    printf("Reading corpus from %s...\n", corpus_file);
    
    // Count total lines in file for progress bar
    int total_lines = 0;
    while (fgets(line, sizeof(line), file)) {
        total_lines++;
    }
    rewind(file);
    
    // Initialize progress bar for first pass
    ProgressBar bar = progress_bar_init("Tokenizing corpus", total_lines, 30);
    
    // First pass: tokenize all molecules and build initial vocabulary
    while (fgets(line, sizeof(line), file)) {
        // Remove newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // Skip empty lines
        if (strlen(line) == 0) {
            progress_bar_increment(&bar);
            continue;
        }
        
        // Tokenize the molecule
        TokenList* tokens = pre_tokenize(line);
        if (!tokens) {
            progress_bar_increment(&bar);
            continue;
        }
        
        // Add tokens to vocabulary
        for (size_t i = 0; i < tokens->count; ++i) {
            hash_insert_or_increment(vocab, tokens->tokens[i]);
        }
        
        free_token_list(tokens);
        line_count++;
        
        // Update progress bar
        progress_bar_increment(&bar);
    }
    
    // First pass complete
    
    printf("\nProcessed a total of %d molecules.\n", line_count);
    printf("Initial vocabulary size: %d tokens\n", count_unique_tokens(vocab));
    
    // Rewind the file for the second pass
    rewind(file);
    
    // Allocate memory for all tokenized molecules
    TokenList** all_tokens = malloc(sizeof(TokenList*) * line_count);
    int token_count = 0;
    
    // Initialize progress bar for second pass
    ProgressBar bar2 = progress_bar_init("Building token lists", total_lines, 30);
    
    // Second pass: re-tokenize and store all molecules
    while (fgets(line, sizeof(line), file) && token_count < line_count) {
        // Remove newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // Skip empty lines
        if (strlen(line) == 0) {
            progress_bar_increment(&bar2);
            continue;
        }
        
        // Tokenize the molecule
        TokenList* tokens = pre_tokenize(line);
        if (!tokens) {
            progress_bar_increment(&bar2);
            continue;
        }
        
        all_tokens[token_count++] = tokens;
        
        // Update progress bar
        progress_bar_increment(&bar2);
    }
    
    // Second pass complete
    
    fclose(file);
    
    // 3. Perform BPE merges
    char** merges = malloc(sizeof(char*) * num_merges);
    int merge_count = 0;
    
    printf("\nStarting BPE training with %d merges...\n", num_merges);
    
    // Initialize progress bar for BPE merges
    ProgressBar bar3 = progress_bar_init("Performing BPE merges", num_merges, 30);
    
    for (int i = 0; i < num_merges; ++i) {
        // Create a hash table for pair statistics
        HashTable* pair_stats = ht_create(HT_DEFAULT_SIZE);
        
        // Initialize progress bar for pair statistics collection
        ProgressBar bar_pairs = progress_bar_init("Collecting pair statistics", token_count, 30);
        
        // Collect pair statistics from all molecules
        for (int j = 0; j < token_count; ++j) {
            TokenList* tokens = all_tokens[j];
            
            // Skip molecules with fewer than 2 tokens
            if (tokens->count < 2) {
                progress_bar_increment(&bar_pairs);
                continue;
            }
            
            // Count pairs in this molecule
            for (size_t k = 0; k < tokens->count - 1; ++k) {
                char pair_key[256];
                snprintf(pair_key, sizeof(pair_key), "%s %s", tokens->tokens[k], tokens->tokens[k+1]);
                hash_insert_or_increment(pair_stats, pair_key);
            }
            
            // Update pair statistics progress bar
            progress_bar_increment(&bar_pairs);
        }
        
        // Pair statistics collection complete
        
        // Find the best pair and its frequency
        int pair_count = 0;
        const char* best_pair = get_best_pair(pair_stats, &pair_count);
        if (!best_pair) {
            ht_free(pair_stats);
            break;
        }
        
        printf("  Merge %d/%d: Best pair: %s (frequency: %d)\n", i+1, num_merges, best_pair, pair_count);
        
        // Store the merge operation
        merges[merge_count++] = strdup(best_pair);
        
        // Split the pair into first and second tokens
        char* pair_copy = strdup(best_pair);
        char* first = pair_copy;
        char* second = NULL;
        
        // Find the space separating the two tokens
        char* space = strchr(pair_copy, ' ');
        if (!space) {
            free(pair_copy);
            ht_free(pair_stats);
            break;
        }
        
        *space = '\0';  // Split the string
        second = space + 1;
        
        // Create the merged token
        char merged[256];
        snprintf(merged, sizeof(merged), "%s%s", first, second);
        
        // Store the merged token with the correct frequency from pair_stats
        // This ensures the frequency in the JSON file will be accurate
        Ht_item* item = hash_search(vocab, merged);
        if (item) {
            // If it already exists, update its count to the pair frequency
            item->count = pair_count;
        } else {
            // Otherwise add it with the correct frequency
            unsigned int slot = hash(merged, vocab->size);
            Ht_item* new_item = malloc(sizeof(Ht_item));
            new_item->key = strdup(merged);
            new_item->count = pair_count;
            new_item->next = vocab->items[slot];
            vocab->items[slot] = new_item;
            vocab->count++;
        }
        
        // Initialize progress bar for applying merges
        ProgressBar bar_merge = progress_bar_init("Applying merge operation", token_count, 30);
        
        // Apply the merge to all molecules
        for (int j = 0; j < token_count; ++j) {
            TokenList* new_tokens = merge_pair(all_tokens[j], best_pair);
            free_token_list(all_tokens[j]);
            all_tokens[j] = new_tokens;
            
            // Update merge application progress bar
            progress_bar_increment(&bar_merge);
        }
        
        // Merge application complete
        
        // Update the overall BPE merge progress bar
        progress_bar_increment(&bar3);
        
        free(pair_copy);
        ht_free(pair_stats);
    }
    
    printf("BPE training completed with %d merges.\n", merge_count);
    
    // Save the vocabulary and merges in both formats
    char vocab_file[PATH_MAX];
    char vocab_base[PATH_MAX];
    
    // Use the output directory for file paths
    int ret1 = snprintf(vocab_file, sizeof(vocab_file), "%s/vocab_%d.txt", output_directory, num_merges);
    int ret2 = snprintf(vocab_base, sizeof(vocab_base), "%s/vocab_%d", output_directory, num_merges);
    
    // Check for truncation
    if (ret1 < 0 || ret1 >= sizeof(vocab_file) || ret2 < 0 || ret2 >= sizeof(vocab_base)) {
        printf("Error: Path too long for vocabulary files\n");
        return NULL;
    }
    
    save_vocabulary(vocab, merges, merge_count, vocab_file);
    save_vocabulary_json(vocab, merges, merge_count, vocab_base);
    
    printf("Vocabulary saved to %s\n", vocab_file);
    printf("JSON vocabulary saved to %s.json and %s_freq.json\n", vocab_base, vocab_base);
    
    // Free all token lists
    for (int i = 0; i < token_count; ++i) {
        free_token_list(all_tokens[i]);
    }
    free(all_tokens);
    
    // Free the merges
    for (int i = 0; i < merge_count; ++i) {
        free(merges[i]);
    }
    free(merges);
    
    return vocab;
}

void print_usage() {
    printf("Usage:\n");
    printf("  cvocgen                       Display this help message\n");
    printf("  cvocgen -f <corpus_file> -n <num_merges> [-t <type>] [-o <output_dir>]  Train on a corpus file\n");
    printf("  cvocgen -l <vocab_file>       Load and display a vocabulary file\n");
    printf("  cvocgen -j <vocab_json>       Load and display a JSON vocabulary file\n");
    printf("\nOptions:\n");
    printf("  -t, --type <type>              Input format type: 'smiles' or 'selfies' (default: selfies)\n");
    printf("  -o, --output <dir>             Output directory for vocabulary files (default: current directory)\n");
}

// Global variable already declared at the top of the file

int main(int argc, char *argv[]) {
    // Check for command-line arguments
    if (argc > 1) {
        // Process command-line arguments
        if (strcmp(argv[1], "-f") == 0 && argc >= 5 && strcmp(argv[3], "-n") == 0) {
            // Train on corpus file
            const char* corpus_file = argv[2];
            int num_merges = atoi(argv[4]);
            
            if (num_merges < 0) {
                printf("Error: Number of merges must be non-negative\n");
                print_usage();
                return 1;
            }
            
            // Check for optional arguments
            for (int i = 5; i < argc - 1; i++) {
                if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--type") == 0) {
                    if (strcmp(argv[i+1], "smiles") == 0) {
                        input_format_is_smiles = 1;
                    } else if (strcmp(argv[i+1], "selfies") == 0) {
                        input_format_is_smiles = 0;
                    } else {
                        printf("Error: Unknown input format '%s'. Must be 'smiles' or 'selfies'\n", argv[i+1]);
                        print_usage();
                        return 1;
                    }
                }
                else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
                    strncpy(output_directory, argv[i+1], sizeof(output_directory) - 1);
                    output_directory[sizeof(output_directory) - 1] = '\0'; // Ensure null termination
                    
                    // Create the directory if it doesn't exist
                    struct stat st = {0};
                    if (stat(output_directory, &st) == -1) {
                        if (mkdir(output_directory, 0755) == -1) {
                            printf("Error: Could not create output directory '%s': %s\n", 
                                   output_directory, strerror(errno));
                            return 1;
                        }
                        printf("Created output directory: %s\n", output_directory);
                    }
                }
            }
            
            printf("Training BPE on corpus file %s with %d merges (format: %s)\n", 
                   corpus_file, num_merges, input_format_is_smiles ? "SMILES" : "SELFIES");
            HashTable* vocab = train_bpe_from_file(corpus_file, num_merges);
            
            if (vocab) {
                printf("\nVocabulary size: %d tokens\n", count_unique_tokens(vocab));
                ht_free(vocab);
                return 0;
            } else {
                printf("Error: Failed to train BPE on corpus file\n");
                return 1;
            }
        } else if (strcmp(argv[1], "-l") == 0 && argc >= 3) {
            // Load vocabulary file
            const char* vocab_file = argv[2];
            
            printf("Loading vocabulary from %s\n", vocab_file);
            char** loaded_merges = NULL;
            int loaded_merge_count = 0;
            HashTable* loaded_vocab = load_vocabulary(vocab_file, &loaded_merges, &loaded_merge_count);
            
            if (loaded_vocab) {
                printf("Loaded %d merge operations:\n", loaded_merge_count);
                for (int i = 0; i < loaded_merge_count; ++i) {
                    printf("  %d. %s\n", i+1, loaded_merges[i]);
                }
                
                printf("\nLoaded vocabulary (showing first 20 entries):\n");
                int count = 0;
                for (unsigned int i = 0; i < loaded_vocab->size && count < 20; ++i) {
                    Ht_item* item = loaded_vocab->items[i];
                    while (item && count < 20) {
                        printf("  - %s: %d\n", item->key, item->count);
                        item = item->next;
                        count++;
                    }
                }
                printf("Total vocabulary size: %d tokens\n", loaded_vocab->size);
                
                // Free loaded resources
                for (int i = 0; i < loaded_merge_count; ++i) {
                    free(loaded_merges[i]);
                }
                free(loaded_merges);
                ht_free(loaded_vocab);
                return 0;
            } else {
                printf("Failed to load vocabulary from %s\n", vocab_file);
                return 1;
            }
        } else if (strcmp(argv[1], "-j") == 0 && argc >= 3) {
            // Load JSON vocabulary file
            const char* json_file = argv[2];
            
            printf("Loading JSON vocabulary from %s\n", json_file);
            char** loaded_merges = NULL;
            int loaded_merge_count = 0;
            HashTable* loaded_vocab = load_vocabulary_json(json_file, &loaded_merges, &loaded_merge_count);
            
            if (loaded_vocab) {
                printf("Loaded %d merge operations from JSON:\n", loaded_merge_count);
                for (int i = 0; i < loaded_merge_count; ++i) {
                    printf("  %d. %s\n", i+1, loaded_merges[i]);
                }
                
                printf("\nLoaded vocabulary (showing first 20 entries):\n");
                int count = 0;
                for (unsigned int i = 0; i < loaded_vocab->size && count < 20; ++i) {
                    Ht_item* item = loaded_vocab->items[i];
                    while (item && count < 20) {
                        printf("  - %s: %d\n", item->key, item->count);
                        item = item->next;
                        count++;
                    }
                }
                printf("Total vocabulary size: %d tokens\n", loaded_vocab->size);
                
                // Free loaded resources
                for (int i = 0; i < loaded_merge_count; ++i) {
                    free(loaded_merges[i]);
                }
                free(loaded_merges);
                ht_free(loaded_vocab);
                return 0;
            } else {
                printf("Error: Failed to load JSON vocabulary from %s\n", json_file);
                return 1;
            }
        } else {
            print_usage();
            return 1;
        }
    } else {
        // Display help when no arguments are provided
        print_usage();
        return 0;
        
        /* Original default example code preserved as comment
        const char* text = "[C][C]([N])([O])Br[C][C]"; // Added extra [C] for a more interesting best pair
        printf("Original text: %s\n\n", text);

        // 1. Initial tokenization and single merge demonstration
        TokenList* tokens = pre_tokenize(text);
        if (!tokens) {
            return 1;
        }

        printf("Initial Tokens:\n");
        for (size_t i = 0; i < tokens->count; ++i) {
            printf("  - %s\n", tokens->tokens[i]);
        }
        printf("\n");
        */

        /* Rest of the default example code preserved as comment
        // 2. Get pair statistics
        HashTable* pair_stats = get_pair_stats(tokens);
        if (pair_stats) {
            printf("Pair Frequencies:\n");
            print_word_counts(pair_stats);
            printf("\n");

            // Find the best pair and its frequency
            int pair_count = 0;
            const char* best_pair = get_best_pair(pair_stats, &pair_count);
            if (best_pair) {
                printf("Best pair: %s (frequency: %d)\n\n", best_pair, pair_count);
                
                // 4. Merge the best pair
                TokenList* merged_tokens = merge_pair(tokens, best_pair);
                if (merged_tokens) {
                    printf("Tokens after merging '%s':\n", best_pair);
                    for (size_t i = 0; i < merged_tokens->count; ++i) {
                        printf("  - %s\n", merged_tokens->tokens[i]);
                    }
                    free_token_list(merged_tokens);
                }
            }

            ht_free(pair_stats);
        }

        free_token_list(tokens);

        // 5. Full BPE training demonstration
        printf("\n=== Full BPE Training ===\n\n");
        int num_merges = 3;
        const char* vocab_file = "vocab.txt";
        
        // Train and save vocabulary
        HashTable* vocab = train_bpe(text, num_merges);
        if (vocab) {
            printf("Final vocabulary after %d merges:\n", num_merges);
            print_word_counts(vocab);
            ht_free(vocab);
        }
        
        // 6. Load vocabulary demonstration
        printf("\n=== Loading Vocabulary from %s ===\n\n", vocab_file);
        char** loaded_merges = NULL;
        int loaded_merge_count = 0;
        HashTable* loaded_vocab = load_vocabulary(vocab_file, &loaded_merges, &loaded_merge_count);
        
        if (loaded_vocab) {
            printf("Loaded %d merge operations:\n", loaded_merge_count);
            for (int i = 0; i < loaded_merge_count; ++i) {
                printf("  %d. %s\n", i+1, loaded_merges[i]);
            }
            
            printf("\nLoaded vocabulary:\n");
            print_word_counts(loaded_vocab);
            
            // Free loaded resources
            for (int i = 0; i < loaded_merge_count; ++i) {
                free(loaded_merges[i]);
            }
            free(loaded_merges);
            ht_free(loaded_vocab);
        } else {
            printf("Failed to load vocabulary from %s\n", vocab_file);
        }
        */
    }

    return 0;
}
