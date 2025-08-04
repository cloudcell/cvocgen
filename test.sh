#!/bin/bash

# Script to compare APETokenizer and cvocgen outputs
# Created: August 4, 2025

# Set error handling
set -e

# Default values
OUTPUT_DIR="./test_results"
MERGES=5

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --merges)
      MERGES="$2"
      shift 2
      ;;
    --output-dir)
      OUTPUT_DIR="$2"
      shift 2
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--merges N] [--output-dir DIR]"
      exit 1
      ;;
  esac
done

echo "Starting tokenizer comparison..."
echo "Number of merges: $MERGES"
echo "Output directory: $OUTPUT_DIR"

# Run the Python comparison script
PYTHONPATH="$(pwd):$PYTHONPATH" python3 tests/compare_tokenizers.py \
  --output-dir "$OUTPUT_DIR" \
  --keep-files \
  --merges "$MERGES"

echo "Comparison complete. Results are in $OUTPUT_DIR/"
