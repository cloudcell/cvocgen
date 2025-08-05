# Fast C-Based Vocabulary Generator Project

This repository contains a C-based vocabulary generator (cvocgen) for molecular representations (SMILES and SELFIES) to be used with Python-based APETokenizer. It implements BPE (Byte Pair Encoding) algorithms optimized for chemical structures, focusing solely on efficient vocabulary generation.

The preliminary measurements show that cvocgen is over 3 times faster than the Python-based APETokenizer for vocabulary generation.

The APETokenizer component is a fork from [Miguelangel Leon Mayuare's original repository](https://github.com/mikemayuare/apetokenizer), with additional enhancements and modifications. It has been forked to make tests for cvocgen and some enhancements to the code.

## Repository Structure

- **apetokenizer/**: Python implementation of the APE (Atom Pair Encoding) Tokenizer
  - `ape_tokenizer.py`: Main tokenizer implementation
  - `README.md`: Documentation for the Python tokenizer
  - `LICENCE`: License information for the Python tokenizer

- **src/**: C implementation of the BPE vocabulary generator
  - `cvocgen.c`: Main C source file
  - `cvocgen.h`: Header file with type definitions and function prototypes
  - `cvocgen_io.h`: I/O utilities for the vocabulary generator
  - `progress_bar.h`: Progress bar implementation
  - `Makefile`: Build configuration that compiles to ../bin/

- **bin/**: Directory for compiled executables
  - `cvocgen`: Compiled C vocabulary generator executable

- **data/**: Sample data files for testing and training
  - Contains test data in a packed file in SELFIES format

- **tests/**: Test scripts and comparison utilities
  - `compare_tokenizers.py`: Script to compare outputs from both tokenizers

- **Root files**:
  - `test.sh`: Shell script to run comparison tests
  - `requirements.txt`: Python dependencies

## Features

### cvocgen (C Vocabulary Generator)

- Fast C implementation of BPE for molecular vocabulary generation
- Command-line interface for efficient vocabulary training
- Configurable output directory for all generated files
- JSON and plain text vocabulary output formats
- Progress bar visualization for training

## Installation

### Prerequisites

- Python 3.8+
- C compiler (gcc recommended)
- Make

### Setup

1. Clone the repository:
   ```bash
   git clone https://github.com/cloudcell/cvocgen.git
   cd cvocgen
   ```

2. Install Python dependencies:
   ```bash
   pip install -r requirements.txt
   ```

3. Build the C vocabulary generator:
   ```bash
   cd src
   make
   cd ..
   ```
   This will compile the executable to the `./bin/` directory.

## Usage

### cvocgen

The C-based vocabulary generator can be used as follows:

```bash
# Build the vocabulary generator
cd src
make
cd ..

# Run with default parameters
(you might have to unpack the test file first)
./bin/cvocgen -i data/test.selfies.unique.txt -n 10 -o ./output_directory

# Options:
# -i, --input: Input file path
# -n, --num-merges: Number of BPE merges to perform
# -o, --output: Output directory for vocabulary files
```

### Running Tests

```bash
# Run the comparison test script
./test.sh --merges 10 --output-dir ./comparison_output
```

## Testing

### C Vocabulary Generator Tests

```bash
# Make sure the vocabulary generator is built first
cd src
make
cd ..

# Run the tests
./test.sh
```

This will:
1. Build the C vocabulary generator
2. Create sample input files in the tests directory
3. Run BPE training with different merge counts
4. Verify vocabulary files are created correctly

### Vocabulary Generator Comparison Tests

```bash
./test.sh --merges 10 --output-dir ./test_output
```

This will:
1. Run both APETokenizer and cvocgen on the same input
2. Compare their vocabularies and outputs
3. Generate a detailed comparison report

### Encoding Length Comparison Tests

```bash
python ./tests/check_encoded_length.py --merges 30 --sample-size 10000
```

This script:
1. Generates vocabularies with both APETokenizer and cvocgen (with specified number of merges)
2. Encodes the same test corpus using both vocabularies (using APETokenizer's encode function)
3. Compares encoding lengths (min, max, average, median) between the two
4. Generates a histogram visualization of encoding length distributions
5. Outputs detailed statistics and total encoding length comparison

Options:
- `--merges`: Number of BPE merges to perform (default: 30)
- `--input-file`: Path to input file (default: ./data/test.selfies.unique.txt)
- `--output-dir`: Directory for output files (default: ./test_results)
- `--sample-size`: Number of molecules to sample (default: all)


## Known Issues / Solutions

- cvocgen has only been tested with SELFIES format
- APETokenizer has a specific frequency calculation where merged token frequencies can be higher than individual token frequencies, which affects BPE merge decisions (solution: use cvocgen for vocabulary generation and APETokenizer for encoding)
  <img width="1200" height="600" alt="image" src="https://github.com/user-attachments/assets/7fabc137-8d14-40dd-b68e-6641d9e2e2a2" />


## License

- APETokenizer: MIT License (see apetokenizer/README.md)
- cvocgen: MIT License (Copyright © 2025 Cloudcell Limited)
- Repository: See individual component licenses
