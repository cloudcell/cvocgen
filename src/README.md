# C Vocabulary Generator (cvocgen)

A C implementation of a vocabulary generator for chemical SMILES strings using Byte Pair Encoding (BPE).

## Features

- Pre-tokenization of chemical SMILES strings using POSIX regex
- Word frequency counting using hash tables
- Pair statistics calculation and best pair selection
- BPE merge operations
- Vocabulary serialization (save/load) in both text and JSON formats
- File-based corpus training
- Command-line interface

## Usage

### Basic Usage

```bash
# Run the default example with a simple chemical string
./cvocgen

# Train on a corpus file with a specified number of merges
./cvocgen -f <corpus_file> -n <num_merges>

# Load and display a vocabulary file
./cvocgen -l <vocab_file>

# Load and display a JSON vocabulary file
./cvocgen -j <vocab_json>
```

### Examples

```bash
# Train on a corpus with 100 merges
./cvocgen -f ../data/test.full -n 100

# Load and view the resulting vocabulary
./cvocgen -l vocab_100.txt
```

## File Formats

### Text Format

The vocabulary text file format is a simple text format:

```
<number_of_merges>
<merge_1>
<merge_2>
...
<merge_n>
---VOCABULARY---
<token_1> <count_1>
<token_2> <count_2>
...
<token_m> <count_m>
```

### JSON Format

The vocabulary is also saved in JSON format as two separate files:

1. **Vocabulary file** (`<basename>.json`):
```json
{
  "<token_1>": <index_1>,
  "<token_2>": <index_2>,
  ...
}
```

2. **Frequency file** (`<basename>_freq.json`):
```json
{
  "<token_1>": <count_1>,
  "<token_2>": <count_2>,
  ...
}
```

## Implementation Details

- Uses POSIX regex.h for pre-tokenization
- Implements a hash table for frequency counting
- Dynamically manages token lists with capacity management
- Careful memory management for all dynamically allocated resources

## Building

```bash
# Build the project
make

# Clean the project
make clean
```

## Performance Considerations

- The implementation is designed to handle large corpus files
- Progress indicators are shown during training
- Memory usage is optimized for large datasets
