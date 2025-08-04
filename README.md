# Molecular Tokenizer Project

This repository contains a Python-based tokenizer (APETokenizer) and a C-based vocabulary generator (cvocgen) for molecular representations (SMILES and SELFIES). Both implement BPE (Byte Pair Encoding) algorithms optimized for chemical structures, but serve different purposes: APETokenizer is a full tokenizer with encoding/decoding capabilities, while cvocgen focuses solely on efficient vocabulary generation.

The APETokenizer is a fork from [Miguelangel Leon Mayuare's original repository](https://github.com/mikemayuare/apetokenizer), with additional enhancements and modifications.

## Repository Structure

- **apetokenizer/**: Python implementation of the APE (Atom Pair Encoding) Tokenizer
  - `ape_tokenizer.py`: Main tokenizer implementation
  - `README.md`: Documentation for the Python tokenizer
  - `LICENCE`: License information for the Python tokenizer
  - `tokenizer_output/`: Directory containing sample tokenizer outputs

- **cvocgen/**: C implementation of the BPE vocabulary generator
  - `cvocgen.c`: Main C source file
  - `cvocgen.h`: Header file with type definitions and function prototypes
  - `cvocgen_io.h`: I/O utilities for the vocabulary generator
  - `progress_bar.h`: Progress bar implementation
  - `Makefile`: Build configuration
  - `test.sh`: Test script
  - `README.md`: Documentation for the C vocabulary generator

- **data/**: Sample data files for testing and training
  - Contains various test files in SMILES and SELFIES formats

- **tests/**: Test scripts and comparison utilities
  - `compare_tokenizers.py`: Script to compare outputs from both tokenizers

- **Root files**:
  - `test.sh`: Shell script to run comparison tests
  - `requirements.txt`: Python dependencies

## Features

### APETokenizer (Python)

- Hugging Face `transformers` compatible
- Tokenizes both SMILES and SELFIES representations
- Supports special tokens (`<pad>`, `<s>`, `</s>`, `<unk>`, `<mask>`)
- Vocabulary management, tokenization, padding, and encoding
- Persistent shared memory for improved multiprocessing performance

### cvocgen (C Vocabulary Generator)

- Fast C implementation of BPE for molecular vocabulary generation
- Command-line interface for efficient vocabulary training
- Configurable output directory for all generated files
- JSON and plain text vocabulary output formats
- Progress bar visualization for training
- Focuses solely on vocabulary generation, not tokenization

## Installation

### Prerequisites

- Python 3.8+
- C compiler (gcc recommended)
- Make

### Setup

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd <repository-directory>
   ```

2. Install Python dependencies:
   ```bash
   pip install -r requirements.txt
   ```

3. Build the C tokenizer:
   ```bash
   cd cvocgen
   make
   cd ..
   ```

## Usage

### APETokenizer

```python
from apetokenizer.ape_tokenizer import APETokenizer

# Initialize the tokenizer
tokenizer = APETokenizer()

# Train on a corpus
corpus = ["CCO", "C=O", "CCC", "CCN"]
tokenizer.train(corpus, max_vocab_size=5000, min_freq_for_merge=2000, max_merges=10)

# Save the vocabulary
tokenizer.save_vocabulary("vocab.json")

# Tokenize a molecule
smiles = "CCO"
encoded = tokenizer(smiles, add_special_tokens=False)
print(encoded)
```

### cvocgen

```bash
# Build the tokenizer
cd cvocgen
make

# Run with default parameters
./cvocgen -i input_file.txt -n 10 -o output_directory

# Options:
# -i, --input: Input file path
# -n, --num-merges: Number of BPE merges to perform
# -o, --output: Output directory for vocabulary files
```

### Comparing Tokenizers

```bash
# Compare both tokenizers with the same input
python tests/compare_tokenizers.py --input-file data/test.selfies.unique.txt --output-dir ./comparison_output --merges 10

# Or use the convenience script
./test.sh --merges 10 --output-dir ./comparison_output
```

## Testing

### C Tokenizer Tests

```bash
cd cvocgen
./test.sh
```

This will:
1. Build the C tokenizer
2. Create sample input files in the tests directory
3. Run BPE training with different merge counts
4. Verify vocabulary files are created correctly

### Tokenizer Comparison Tests

```bash
./test.sh --merges 10 --output-dir ./test_output
```

This will:
1. Run both tokenizers on the same input
2. Compare their vocabularies and outputs
3. Generate a detailed comparison report


## Known Issues

- APETokenizer has unintuitive frequency calculation where merged token frequencies can be higher than individual token frequencies, which affects BPE merge decisions

## License

- APETokenizer: MIT License (see apetokenizer/README.md)
- cvocgen: MIT License (Copyright © 2025 Cloudcell Limited)
- Repository: See individual component licenses
