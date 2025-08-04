#ifndef CVOCGEN_IO_H
#define CVOCGEN_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cvocgen.h"

// External function declarations needed for this header
extern unsigned int hash(const char* key, unsigned int size);

// Helper function to escape strings for JSON output
static inline char* json_escape_string(const char* input) {
    if (!input) return NULL;
    
    // Count the number of characters that need escaping to determine output size
    size_t escape_count = 0;
    const char* p = input;
    while (*p) {
        // Characters that need escaping in JSON: ", \, /, \b, \f, \n, \r, \t
        if (*p == '"' || *p == '\\' || *p == '/' || 
            *p == '\b' || *p == '\f' || *p == '\n' || *p == '\r' || *p == '\t') {
            escape_count++;
        }
        p++;
    }
    
    // Allocate memory for the escaped string (each escaped char becomes two)
    size_t input_len = strlen(input);
    size_t output_len = input_len + escape_count + 1; // +1 for null terminator
    char* output = (char*)malloc(output_len);
    if (!output) return NULL;
    
    // Copy and escape
    size_t j = 0;
    for (size_t i = 0; i < input_len; i++) {
        switch (input[i]) {
            case '"':
                output[j++] = '\\';
                output[j++] = '"';
                break;
            case '\\':
                output[j++] = '\\';
                output[j++] = '\\';
                break;
            case '/':
                output[j++] = '\\';
                output[j++] = '/';
                break;
            case '\b':
                output[j++] = '\\';
                output[j++] = 'b';
                break;
            case '\f':
                output[j++] = '\\';
                output[j++] = 'f';
                break;
            case '\n':
                output[j++] = '\\';
                output[j++] = 'n';
                break;
            case '\r':
                output[j++] = '\\';
                output[j++] = 'r';
                break;
            case '\t':
                output[j++] = '\\';
                output[j++] = 't';
                break;
            default:
                output[j++] = input[i];
                break;
        }
    }
    output[j] = '\0';
    
    return output;
}

// Save vocabulary and merges to a text file
static inline int save_vocabulary(HashTable* vocab, char** merges, int merge_count, const char* filename) {
    if (!vocab || !filename) {
        return -1;
    }

    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Error opening file for writing");
        return -1;
    }

    // Write the number of merges
    fprintf(file, "%d\n", merge_count);

    // Write the merge operations
    for (int i = 0; i < merge_count; ++i) {
        fprintf(file, "%s\n", merges[i]);
    }

    // Write the vocabulary
    fprintf(file, "---VOCABULARY---\n");
    for (unsigned int i = 0; i < vocab->size; ++i) {
        Ht_item* item = vocab->items[i];
        while (item) {
            fprintf(file, "%s\t%d\n", item->key, item->count);
            item = item->next;
        }
    }

    fclose(file);
    return 0;
}

// Save vocabulary and merges to JSON files with simplified format
static inline int save_vocabulary_json(HashTable* vocab, char** merges, int merge_count, const char* base_filename) {
    if (!vocab || !base_filename) {
        return -1;
    }
    
    // Create filenames for vocabulary and frequency JSON files
    char vocab_filename[256];
    char freq_filename[256];
    snprintf(vocab_filename, sizeof(vocab_filename), "%s.json", base_filename);
    snprintf(freq_filename, sizeof(freq_filename), "%s_freq.json", base_filename);
    
    // Open vocabulary JSON file
    FILE* vocab_file = fopen(vocab_filename, "w");
    if (!vocab_file) {
        perror("Error opening vocabulary JSON file for writing");
        return -1;
    }
    
    // Open frequency JSON file
    FILE* freq_file = fopen(freq_filename, "w");
    if (!freq_file) {
        perror("Error opening frequency JSON file for writing");
        fclose(vocab_file);
        return -1;
    }
    
    // Start JSON objects with a single set of curly braces
    fprintf(vocab_file, "{\n");
    fprintf(freq_file, "{\n");
    
    // Special tokens first with fixed indices (0-4)
    const char* special_tokens[] = {"<s>", "<pad>", "</s>", "<unk>", "<mask>"};
    int special_token_count = 5;
    int index = 0;
    
    // Add special tokens to vocabulary file
    for (int i = 0; i < special_token_count; i++) {
        fprintf(vocab_file, "  \"%s\": %d%s\n", 
                special_tokens[i], index++, 
                (i < special_token_count - 1 || vocab->size > 0 || merge_count > 0) ? "," : "");
    }
    
    // Count total items for proper JSON formatting (to handle commas)
    int total_items = 0;
    for (unsigned int i = 0; i < vocab->size; ++i) {
        Ht_item* item = vocab->items[i];
        while (item) {
            // Skip special tokens that were already added
            int is_special = 0;
            for (int j = 0; j < special_token_count; j++) {
                if (strcmp(item->key, special_tokens[j]) == 0) {
                    is_special = 1;
                    break;
                }
            }
            if (!is_special) total_items++;
            item = item->next;
        }
    }
    
    // Add merge operations to the count
    total_items += merge_count;
    
    // Create a list of tokens to write (excluding special tokens)
    char** token_list = NULL;
    int* count_list = NULL;
    int token_count = 0;
    int max_tokens = 100;
    
    // Allocate initial arrays
    token_list = (char**)malloc(sizeof(char*) * max_tokens);
    count_list = (int*)malloc(sizeof(int) * max_tokens);
    
    if (!token_list || !count_list) {
        if (token_list) free(token_list);
        if (count_list) free(count_list);
        fclose(vocab_file);
        fclose(freq_file);
        return -1;
    }
    
    // Collect all tokens and counts first
    for (unsigned int i = 0; i < vocab->size; ++i) {
        Ht_item* item = vocab->items[i];
        while (item) {
            // Skip special tokens that were already added
            int is_special = 0;
            for (int j = 0; j < special_token_count; j++) {
                if (strcmp(item->key, special_tokens[j]) == 0) {
                    is_special = 1;
                    break;
                }
            }
            
            if (!is_special) {
                // Resize arrays if needed
                if (token_count >= max_tokens) {
                    max_tokens *= 2;
                    char** new_tokens = (char**)realloc(token_list, sizeof(char*) * max_tokens);
                    int* new_counts = (int*)realloc(count_list, sizeof(int) * max_tokens);
                    
                    if (!new_tokens || !new_counts) {
                        // Free everything on error
                        for (int j = 0; j < token_count; j++) {
                            free(token_list[j]);
                        }
                        free(token_list);
                        free(count_list);
                        fclose(vocab_file);
                        fclose(freq_file);
                        return -1;
                    }
                    
                    token_list = new_tokens;
                    count_list = new_counts;
                }
                
                // Add token and count to lists
                token_list[token_count] = strdup(item->key);
                count_list[token_count] = item->count;
                token_count++;
            }
            
            item = item->next;
        }
    }
    
    // Now write tokens to files with proper comma handling
    for (int i = 0; i < token_count; i++) {
        // Escape the token for JSON
        char* escaped_token = json_escape_string(token_list[i]);
        if (!escaped_token) {
            // Handle error
            for (int j = i; j < token_count; j++) {
                free(token_list[j]);
            }
            free(token_list);
            free(count_list);
            fclose(vocab_file);
            fclose(freq_file);
            return -1;
        }
        
        // For vocabulary JSON: token -> index mapping
        fprintf(vocab_file, "  \"%s\": %d%s\n", 
                escaped_token, index++, (i < token_count - 1) ? "," : "");
        
        // For frequency JSON: token -> count mapping
        fprintf(freq_file, "  \"%s\": %d%s\n", 
                escaped_token, count_list[i], 
                (i < token_count - 1) ? "," : "");
        
        // Free escaped token and original token
        free(escaped_token);
        free(token_list[i]);
    }
    
    // Free arrays
    free(token_list);
    free(count_list);
    
    // Add merge operations as regular tokens
    // for (int i = 0; i < merge_count; i++) {
    //     // Escape the merge operation for JSON
    //     char* escaped_merge = json_escape_string(merges[i]);
    //     if (!escaped_merge) {
    //         // Handle error
    //         fclose(vocab_file);
    //         fclose(freq_file);
    //         return -1;
    //     }
        
    //     // Get the merged token from the vocabulary hash table
    //     char* pair_copy = strdup(merges[i]);
    //     char* space = strchr(pair_copy, ' ');
    //     if (space) {
    //         *space = '\0';  // Split the string
    //         char* second = space + 1;
    //         char merged[256];
    //         snprintf(merged, sizeof(merged), "%s%s", pair_copy, second);
            
    //         // Look up the merged token in the vocabulary
    //         Ht_item* item = hash_search(vocab, merged);
    //         if (item) {
    //             // Use the key from the vocabulary hash table
    //             free(escaped_merge);
    //             escaped_merge = json_escape_string(item->key);
    //         }
    //         free(pair_copy);
    //     }
        
    //     // Add to vocabulary JSON with index
    //     fprintf(vocab_file, "  \"%s\": %d%s\n", 
    //             escaped_merge, index++, (i < merge_count - 1) ? "," : "");
        
    //     // Add to frequency JSON with a proper count
    //     // Look up the merged token in the vocabulary to get its actual frequency
    //     Ht_item* merged_item = hash_search(vocab, merges[i]);
    //     int freq = merged_item ? merged_item->count : 1; // Use 1 as fallback if not found
        
    //     // Add comma if this isn't the last merge
    //     fprintf(freq_file, "  \"%s\": %d%s\n", 
    //             escaped_merge, freq, (i < merge_count - 1) ? "," : "");
        
    //     // Free escaped merge
    //     free(escaped_merge);
    // }
    
    // Close JSON objects
    fprintf(vocab_file, "}\n");
    fprintf(freq_file, "}\n");
    
    // Close files
    fclose(vocab_file);
    fclose(freq_file);
    
    printf("Vocabulary saved to %s\n", vocab_filename);
    printf("Frequencies saved to %s\n", freq_filename);
    
    return 0;
}

// Load vocabulary and merges from a file
static inline HashTable* load_vocabulary(const char* filename, char*** merges_out, int* merge_count_out) {
    if (!filename || !merges_out || !merge_count_out) {
        return NULL;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file for reading");
        return NULL;
    }

    // Read the number of merges
    int merge_count;
    if (fscanf(file, "%d\n", &merge_count) != 1) {
        fclose(file);
        return NULL;
    }

    // Allocate memory for merges
    char** merges = malloc(sizeof(char*) * merge_count);
    if (!merges) {
        fclose(file);
        return NULL;
    }

    // Read the merge operations
    char line[256];
    for (int i = 0; i < merge_count; ++i) {
        if (fgets(line, sizeof(line), file) == NULL) {
            // Error reading line
            for (int j = 0; j < i; ++j) {
                free(merges[j]);
            }
            free(merges);
            fclose(file);
            return NULL;
        }
        // Remove newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        merges[i] = strdup(line);
    }

    // Look for the vocabulary marker
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strcmp(line, "---VOCABULARY---\n") == 0) {
            break;
        }
    }

    // Create vocabulary hash table
    HashTable* vocab = ht_create(100);

    // Read the vocabulary entries
    char token[256];
    int count;
    while (fscanf(file, "%255s\t%d\n", token, &count) == 2) {
        Ht_item* item = hash_search(vocab, token);
        if (item) {
            item->count = count;
        } else {
            unsigned int slot = hash(token, vocab->size);
            Ht_item* new_item = malloc(sizeof(Ht_item));
            new_item->key = strdup(token);
            new_item->count = count;
            new_item->next = vocab->items[slot];
            vocab->items[slot] = new_item;
        }
    }

    fclose(file);
    *merges_out = merges;
    *merge_count_out = merge_count;
    return vocab;
}

// Load vocabulary and merges from a simplified JSON file
static inline HashTable* load_vocabulary_json(const char* json_filename, char*** merges_out, int* merge_count_out) {
    if (!json_filename || !merges_out || !merge_count_out) {
        return NULL;
    }
    
    FILE* file = fopen(json_filename, "r");
    if (!file) {
        perror("Error opening JSON file for reading");
        return NULL;
    }
    
    // Create vocabulary hash table
    HashTable* vocab = ht_create(100);
    
    // Read entire file into memory for easier parsing
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    
    char* json_content = (char*)malloc(file_size + 1);
    if (!json_content) {
        perror("Memory allocation failed");
        fclose(file);
        ht_free(vocab);
        return NULL;
    }
    
    size_t bytes_read = fread(json_content, 1, file_size, file);
    json_content[bytes_read] = '\0'; // Null-terminate
    fclose(file);
    
    // Initialize merge count
    int merge_count = 0;
    
    // Allocate initial merges array (we'll realloc if needed)
    int max_merges = 100;
    char** merges = (char**)malloc(sizeof(char*) * max_merges);
    if (!merges) {
        free(json_content);
        ht_free(vocab);
        return NULL;
    }
    
    // Parse JSON content - simplified format with just key-value pairs
    char* ptr = json_content;
    
    // Skip opening brace
    while (*ptr && *ptr != '{') ptr++;
    if (*ptr == '{') ptr++;
    
    // Parse key-value pairs
    while (*ptr) {
        // Skip whitespace
        while (*ptr && isspace(*ptr)) ptr++;
        if (!*ptr || *ptr == '}') break;
        
        // Check for key start
        if (*ptr == '"') {
            ptr++; // Skip opening quote
            char* key_start = ptr;
            
            // Find end of key
            while (*ptr && *ptr != '"') ptr++;
            if (!*ptr) break;
            
            // Extract key
            size_t key_len = ptr - key_start;
            char* key = (char*)malloc(key_len + 1);
            if (!key) continue;
            
            strncpy(key, key_start, key_len);
            key[key_len] = '\0';
            
            ptr++; // Skip closing quote
            
            // Skip to value
            while (*ptr && *ptr != ':') ptr++;
            if (!*ptr) {
                free(key);
                break;
            }
            
            ptr++; // Skip colon
            
            // Skip whitespace
            while (*ptr && isspace(*ptr)) ptr++;
            if (!*ptr) {
                free(key);
                break;
            }
            
            // Check if value is a string (for merge operations)
            if (*ptr == '"') {
                ptr++; // Skip opening quote
                char* value_start = ptr;
                
                // Find end of string value
                while (*ptr && *ptr != '"') ptr++;
                if (!*ptr) {
                    free(key);
                    break;
                }
                
                // Extract string value
                size_t value_len = ptr - value_start;
                char* value = (char*)malloc(value_len + 1);
                if (!value) {
                    free(key);
                    continue;
                }
                
                strncpy(value, value_start, value_len);
                value[value_len] = '\0';
                
                // Check if it's a merge operation (contains a space)
                if (strchr(value, ' ')) {
                    // Store as merge operation
                    if (merge_count >= max_merges) {
                        max_merges *= 2;
                        char** new_merges = (char**)realloc(merges, sizeof(char*) * max_merges);
                        if (!new_merges) {
                            free(key);
                            free(value);
                            continue;
                        }
                        merges = new_merges;
                    }
                    merges[merge_count++] = strdup(value);
                } else {
                    // Add to vocabulary with placeholder count
                    unsigned int slot = hash(key, vocab->size);
                    Ht_item* new_item = malloc(sizeof(Ht_item));
                    new_item->key = strdup(key);
                    new_item->count = 1; // Placeholder count
                    new_item->next = vocab->items[slot];
                    vocab->items[slot] = new_item;
                }
                
                free(value);
                free(key);
                
                ptr++; // Skip closing quote
            } else if (isdigit(*ptr) || *ptr == '-') { // Numeric value
                // Parse number
                int value = atoi(ptr);
                
                // Skip past number
                while (*ptr && (isdigit(*ptr) || *ptr == '-' || *ptr == '.')) ptr++;
                
                // Add to vocabulary with index as count
                unsigned int slot = hash(key, vocab->size);
                Ht_item* new_item = malloc(sizeof(Ht_item));
                new_item->key = strdup(key);
                new_item->count = value;
                new_item->next = vocab->items[slot];
                vocab->items[slot] = new_item;
                
                free(key);
            } else {
                free(key);
            }
            
            // Skip to next pair
            while (*ptr && *ptr != ',' && *ptr != '}') ptr++;
            if (*ptr == ',') ptr++;
        } else {
            ptr++;
        }
    }
    
    // Clean up file resources
    free(json_content);
    
    // Try to load frequencies from _freq.json file
    char freq_filename[256];
    char* json_base = strdup(json_filename);
    char* ext = strstr(json_base, ".json");
    if (ext) *ext = '\0';
    snprintf(freq_filename, sizeof(freq_filename), "%s_freq.json", json_base);
    free(json_base);
    
    FILE* freq_file = fopen(freq_filename, "r");
    if (freq_file) {
        // Read entire frequency file into memory
        fseek(freq_file, 0, SEEK_END);
        long freq_file_size = ftell(freq_file);
        rewind(freq_file);
        
        char* freq_content = (char*)malloc(freq_file_size + 1);
        if (freq_content) {
            size_t bytes_read = fread(freq_content, 1, freq_file_size, freq_file);
            freq_content[bytes_read] = '\0'; // Null-terminate
            fclose(freq_file);
            
            // Parse frequency JSON content - simplified format
            char* ptr = freq_content;
            
            // Skip opening brace
            while (*ptr && *ptr != '{') ptr++;
            if (*ptr == '{') ptr++;
            
            // Parse key-value pairs
            while (*ptr) {
                // Skip whitespace
                while (*ptr && isspace(*ptr)) ptr++;
                if (!*ptr || *ptr == '}') break;
                
                // Check for key start
                if (*ptr == '"') {
                    ptr++; // Skip opening quote
                    char* key_start = ptr;
                    
                    // Find end of key
                    while (*ptr && *ptr != '"') ptr++;
                    if (!*ptr) break;
                    
                    // Extract key
                    size_t key_len = ptr - key_start;
                    char* key = (char*)malloc(key_len + 1);
                    if (!key) continue;
                    
                    strncpy(key, key_start, key_len);
                    key[key_len] = '\0';
                    
                    ptr++; // Skip closing quote
                    
                    // Skip to value
                    while (*ptr && *ptr != ':') ptr++;
                    if (!*ptr) {
                        free(key);
                        break;
                    }
                    
                    ptr++; // Skip colon
                    
                    // Skip whitespace
                    while (*ptr && isspace(*ptr)) ptr++;
                    if (!*ptr) {
                        free(key);
                        break;
                    }
                    
                    // Parse number
                    if (isdigit(*ptr) || *ptr == '-') {
                        int count = atoi(ptr);
                        
                        // Update count in vocabulary
                        Ht_item* item = hash_search(vocab, key);
                        if (item) {
                            item->count = count;
                        }
                    }
                    
                    free(key);
                    
                    // Skip to next pair
                    while (*ptr && *ptr != ',' && *ptr != '}') ptr++;
                    if (*ptr == ',') ptr++;
                } else {
                    ptr++;
                }
            }
            
            free(freq_content);
        } else {
            fclose(freq_file);
        }
    }
    
    *merges_out = merges;
    *merge_count_out = merge_count;
    return vocab;
}

#endif /* CVOCGEN_IO_H */
