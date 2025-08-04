#!/usr/bin/env python3
"""
Compare the output of APETokenizer and cvocgen on the same input file.
This script runs both tokenizers with similar parameters and compares their results.
"""

import os
import json
import subprocess
import tempfile
import shutil
import re
import gzip
import traceback
from collections import defaultdict
import argparse
from apetokenizer.ape_tokenizer import APETokenizer

def load_corpus(file_path):
    """Load corpus from a file."""
    with open(file_path, 'r', encoding='utf-8') as f:
        return [line.strip() for line in f.readlines()]

def run_apetokenizer(input_file, output_dir, num_merges=1):
    """Run APETokenizer on the input file and return the vocabulary."""
    print(f"Running APETokenizer with --merge {num_merges}...")
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Create tokenizer instance
    tokenizer = APETokenizer()
    
    # Load corpus
    corpus = load_corpus(input_file)
    
    # Train the tokenizer
    tokenizer.train(
        corpus=corpus,
        type="selfies",  # Assuming SELFIES format
        max_vocab_size=5000,
        min_freq_for_merge=2000,
        max_merges=num_merges
    )
    
    # Save the vocabulary
    vocab_path = os.path.join(output_dir, "ape_vocab.json")
    tokenizer.save_vocabulary(vocab_path)
    
    # Print some info about the vocabulary
    print(f"APETokenizer vocabulary size: {len(tokenizer.vocabulary)}")
    print(f"Special tokens: {tokenizer.special_tokens}")
    
    # Convert to frequency dictionary (excluding special tokens)
    freq_dict = {}
    for token, idx in tokenizer.vocabulary.items():
        if token not in tokenizer.special_tokens:
            freq_dict[token] = tokenizer.vocabulary_frequency.get(token, 0)
    
    # Print some sample tokens
    print("\nSample APETokenizer tokens:")
    for i, (token, freq) in enumerate(sorted(freq_dict.items(), key=lambda x: x[1], reverse=True)):
        if i >= 5:
            break
        print(f"{token}: {freq}")
    
    return freq_dict

def run_cvocgen(input_file, output_dir, num_merges=1):
    """Run cvocgen on the input file and return the vocabulary."""
    print(f"\nRunning cvocgen with -n {num_merges}...")
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Run cvocgen command with output directory
    cmd = [
        "./cvocgen/cvocgen",
        "-f", input_file,
        "-n", str(num_merges),
        "-t", "selfies",
        "-o", output_dir
    ]
    
    try:
        # Run cvocgen with stdout directly to terminal for real-time output
        result = subprocess.run(
            cmd,
            text=True,
            check=True
        )
        print("cvocgen execution completed successfully")
    except subprocess.CalledProcessError as e:
        print(f"Error running cvocgen: {e}")
        print(f"stdout: {e.stdout}")
        print(f"stderr: {e.stderr}")
        return {}
    
    # Look for numbered vocabulary files (vocab_1.json, vocab_1_freq.json, etc.) in the output directory
    numbered_vocab_files = []
    for file in os.listdir(output_dir):
        if file.startswith(f"vocab_{num_merges}") and file.endswith(".json"):
            numbered_vocab_files.append(os.path.join(output_dir, file))
    
    if not numbered_vocab_files:
        print(f"Error: cvocgen did not create any numbered vocabulary files for {num_merges} merges")
        return {}
    
    print(f"Found numbered vocabulary files: {numbered_vocab_files}")
    
    # Determine which file to use for frequency data
    freq_file = None
    for file in numbered_vocab_files:
        if file.endswith("_freq.json"):
            freq_file = file
            break
    
    if not freq_file:
        # If no _freq.json file found, use the first JSON file
        freq_file = numbered_vocab_files[0]
    
    print(f"Using {freq_file} for vocabulary data")
    
    # Files are already in the output directory, no need to copy them
    
    # Load the vocabulary from the JSON file (which is now a full path)
    try:
        with open(freq_file, 'r', encoding='utf-8') as f:
            content = f.read()
            
            # Check if the JSON is missing the closing brace
            if content.strip().endswith(','):
                content = content.strip()[:-1] + '}'
            elif not content.strip().endswith('}'): 
                content = content.strip() + '}'
                
            try:
                vocab_data = json.loads(content)
                print(f"Successfully loaded vocabulary from {freq_file}")
                
                # Convert the vocabulary data to a frequency dictionary
                vocab_freq = {}
                
                # Handle different JSON formats
                if isinstance(vocab_data, dict):
                    # If it's already a dictionary, use it directly
                    vocab_freq = vocab_data
                elif isinstance(vocab_data, list):
                    # If it's a list of tokens, convert to a dictionary with default frequency of 1
                    for token in vocab_data:
                        vocab_freq[token] = 1
                        
            except json.JSONDecodeError as e:
                print(f"Error: Could not parse JSON in {freq_file} even with fixes: {e}")
                # Try a more aggressive approach - parse line by line
                print("Attempting to parse JSON line by line...")
                vocab_freq = {}
                
                # Reset file pointer (freq_file is already a full path)
                with open(freq_file, 'r', encoding='utf-8') as f2:
                    lines = f2.readlines()
                
                # Skip first line (opening brace) and parse each key-value pair
                for line in lines[1:]:  # Skip the first line which should be '{'  
                    line = line.strip()
                    if not line or line == '{' or line == '}': 
                        continue
                        
                    # Remove trailing comma if present
                    if line.endswith(','):
                        line = line[:-1]
                        
                    try:
                        # Try to parse the line as a JSON fragment
                        key_value = '{' + line + '}'
                        pair = json.loads(key_value)
                        # Add the key-value pair to our dictionary
                        for k, v in pair.items():
                            vocab_freq[k] = v
                    except json.JSONDecodeError:
                        # Skip lines that can't be parsed
                        continue
                
                if vocab_freq:
                    print(f"Successfully parsed {len(vocab_freq)} tokens from {freq_file} using line-by-line method")
                    
    except Exception as e:
        print(f"Error reading vocabulary file {freq_file}: {e}")
        return {}
    
    # Print some info about the vocabulary
    print(f"cvocgen vocabulary size: {len(vocab_freq)}")
    
    # Print some sample tokens
    print("\nSample cvocgen tokens:")
    items = list(sorted(vocab_freq.items(), key=lambda x: x[1] if isinstance(x[1], int) else 0, reverse=True))
    for i, (token, freq) in enumerate(items):
        if i >= 5:
            break
        print(f"{token}: {freq}")
    
    return vocab_freq

def compare_vocabularies(ape_vocab, cvoc_vocab):
    """Compare the vocabularies from both tokenizers."""
    print("\n=== Vocabulary Comparison ===")
    
    # Get all unique tokens
    all_tokens = set(ape_vocab.keys()) | set(cvoc_vocab.keys())
    
    # Count statistics
    common_tokens = 0
    only_in_ape = 0
    only_in_cvoc = 0
    different_freq = 0
    
    # Lists to store tokens by category
    common_token_list = []
    ape_only_list = []
    cvoc_only_list = []
    
    # Compare each token
    for token in sorted(all_tokens):
        ape_freq = ape_vocab.get(token, 0)
        cvoc_freq = cvoc_vocab.get(token, 0)
        
        if token in ape_vocab and token in cvoc_vocab:
            status = "Common"
            common_tokens += 1
            common_token_list.append((token, ape_freq, cvoc_freq))
            if ape_freq != cvoc_freq:
                status = "Diff Freq"
                different_freq += 1
        elif token in ape_vocab:
            status = "Only in APE"
            only_in_ape += 1
            ape_only_list.append((token, ape_freq))
        else:
            status = "Only in cvocgen"
            only_in_cvoc += 1
            cvoc_only_list.append((token, cvoc_freq))
    
    # Display token comparison with pagination
    display_limit = 20
    
    print("\nToken-by-token comparison (limited to first 20 tokens):")
    print(f"{'Token':<20} {'APE Freq':<10} {'cvocgen Freq':<10} {'Status':<15}")
    print("-" * 55)
    
    # Display a sample of tokens
    display_count = 0
    for token in sorted(all_tokens):
        if display_count >= display_limit:
            break
            
        ape_freq = ape_vocab.get(token, 0)
        cvoc_freq = cvoc_vocab.get(token, 0)
        
        if token in ape_vocab and token in cvoc_vocab:
            status = "Common"
            if ape_freq != cvoc_freq:
                status = "Diff Freq"
        elif token in ape_vocab:
            status = "Only in APE"
        else:
            status = "Only in cvocgen"
            
        print(f"{token:<20} {ape_freq:<10} {cvoc_freq:<10} {status:<15}")
        display_count += 1
    
    if display_count == display_limit and len(all_tokens) > display_limit:
        print(f"... and {len(all_tokens) - display_limit} more tokens (not shown)")
    
    # Print summary
    print("\n=== Summary ===")
    print(f"Total unique tokens: {len(all_tokens)}")
    print(f"Common tokens: {common_tokens}")
    print(f"Only in APETokenizer: {only_in_ape}")
    print(f"Only in cvocgen: {only_in_cvoc}")
    print(f"Common tokens with different frequencies: {different_freq}")
    
    # Calculate similarity percentage
    if len(all_tokens) > 0:
        similarity = (common_tokens / len(all_tokens)) * 100
        print(f"Vocabulary similarity: {similarity:.2f}%")
    else:
        print("No tokens to compare.")
    
    # Frequency correlation for common tokens
    if common_tokens > 0:
        # Sort common tokens by frequency in APE
        sorted_common = sorted(common_token_list, key=lambda x: x[1], reverse=True)
        
        print("\n=== Top 10 Common Tokens by Frequency ===")
        print(f"{'Token':<20} {'APE Freq':<10} {'cvocgen Freq':<10} {'Ratio (APE/cvocgen)':<20}")
        print("-" * 60)
        
        for i, (token, ape_freq, cvoc_freq) in enumerate(sorted_common[:10]):
            ratio = ape_freq / cvoc_freq if cvoc_freq > 0 else float('inf')
            print(f"{token:<20} {ape_freq:<10} {cvoc_freq:<10} {ratio:.4f}")
    
    # Show examples of tokens only in each tokenizer
    if only_in_ape > 0:
        print("\n=== Examples of Tokens Only in APETokenizer ===")
        sorted_ape_only = sorted(ape_only_list, key=lambda x: x[1], reverse=True)
        for token, freq in sorted_ape_only[:10]:
            print(f"{token}: {freq}")
    
    if only_in_cvoc > 0:
        print("\n=== Examples of Tokens Only in cvocgen ===")
        sorted_cvoc_only = sorted(cvoc_only_list, key=lambda x: x[1], reverse=True)
        for token, freq in sorted_cvoc_only[:10]:
            print(f"{token}: {freq}")
            
    # Analysis of token patterns
    print("\n=== Token Pattern Analysis ===")
    
    # Analyze token lengths
    ape_token_lengths = [len(token) for token in ape_vocab.keys()]
    cvoc_token_lengths = [len(token) for token in cvoc_vocab.keys()]
    
    if ape_token_lengths:
        avg_ape_len = sum(ape_token_lengths) / len(ape_token_lengths)
        print(f"Average APETokenizer token length: {avg_ape_len:.2f} characters")
    
    if cvoc_token_lengths:
        avg_cvoc_len = sum(cvoc_token_lengths) / len(cvoc_token_lengths)
        print(f"Average cvocgen token length: {avg_cvoc_len:.2f} characters")
    
    # Return the comparison statistics for potential further analysis
    return {
        "total_tokens": len(all_tokens),
        "common_tokens": common_tokens,
        "only_in_ape": only_in_ape,
        "only_in_cvoc": only_in_cvoc,
        "different_freq": different_freq,
        "similarity": (common_tokens / len(all_tokens)) * 100 if len(all_tokens) > 0 else 0
    }

def main():
    parser = argparse.ArgumentParser(description="Compare APETokenizer and cvocgen outputs")
    parser.add_argument("--output-dir", type=str, default="./tokenizer_comparison_output", 
                        help="Directory to store output files")
    parser.add_argument("--input-file", type=str, default="./data/test.selfies.unique.txt", 
                        help="Input corpus file")
    parser.add_argument("--keep-files", action="store_true", 
                        help="Keep temporary files")
    parser.add_argument("--merges", type=int, default=1, 
                        help="Number of merges to perform")
    args = parser.parse_args()
    
    # Use the command-line parameters
    output_dir = args.output_dir
    input_file = args.input_file
    num_merges = args.merges
    
    print(f"Using output directory: {output_dir}")
    print(f"Input file: {input_file}")
    print(f"Number of merges: {num_merges}")
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Check if the input file exists, if not, try to unpack from gz file
    if not os.path.exists(input_file):
        print(f"Input file {input_file} not found, checking for gzipped version...")
        gz_file = f"{input_file}.gz"
        if os.path.exists(gz_file):
            print(f"Found gzipped file {gz_file}, unpacking...")
            try:
                # Extract the directory path from input_file
                data_dir = os.path.dirname(input_file)
                # Ensure the directory exists
                os.makedirs(data_dir, exist_ok=True)
                
                # Unpack the gz file
                with gzip.open(gz_file, 'rb') as f_in:
                    with open(input_file, 'wb') as f_out:
                        shutil.copyfileobj(f_in, f_out)
                print(f"Successfully unpacked {gz_file} to {input_file}")
            except Exception as e:
                print(f"Error unpacking gzipped file: {e}")
                traceback.print_exc()
                return
        else:
            print(f"Error: Neither {input_file} nor {gz_file} found")
            return
    
    # Print information about the input file
    try:
        with open(input_file, 'r') as f:
            line_count = len(f.readlines())
        print(f"Number of lines in input file: {line_count}")
    except Exception as e:
        print(f"Error reading input file: {e}")
        traceback.print_exc()
        return
    
    try:
        ape_vocab = run_apetokenizer(input_file, output_dir, num_merges)
        cvoc_vocab = run_cvocgen(input_file, output_dir, num_merges)
        
        # Compare vocabularies
        compare_vocabularies(ape_vocab, cvoc_vocab)
    except Exception as e:
        print(f"Error during tokenizer comparison: {e}")
        traceback.print_exc()
    finally:
        # Clean up temporary files if not keeping them
        if not args.keep_files:
            print("Cleaning up temporary files...")
            try:
                for file in os.listdir(output_dir):
                    os.remove(os.path.join(output_dir, file))
                os.rmdir(output_dir)
            except Exception as e:
                print(f"Error cleaning up files: {e}")
        else:
            print(f"Output files kept in: {output_dir}")
            print("You can examine these files for further analysis.")
            print(f"To clean up manually: rm -rf {output_dir}")

if __name__ == "__main__":
    main()
