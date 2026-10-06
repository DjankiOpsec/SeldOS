#!/usr/bin/env python3
"""
Opsec Oracle - Documentation Corpus Preprocessor
Extracts and sanitizes clean word streams from raw documentation.
Strips markup, URLs, code blocks, frontmatter, and non-prose tokens.
Outputs strictly one word per line into corpus.txt.
"""

import os
import re
import sys
from pathlib import Path

def sanitize_content(text: str) -> str:
    # 1. Remove YAML / TOML frontmatter
    text = re.sub(r'^---[\s\S]*?---\n', '', text, flags=re.MULTILINE)
    text = re.sub(r'^\+\+\+[\s\S]*?\+\+\+\n', '', text, flags=re.MULTILINE)

    # 2. Remove fenced code blocks
    text = re.sub(r'```[\s\S]*?```', ' ', text)
    text = re.sub(r'~~~[\s\S]*?~~~', ' ', text)

    # 3. Remove HTML comments and script/style tags
    text = re.sub(r'<!--[\s\S]*?-->', ' ', text)
    text = re.sub(r'<script[\s\S]*?</script>', ' ', text, flags=re.IGNORECASE)
    text = re.sub(r'<style[\s\S]*?</style>', ' ', text, flags=re.IGNORECASE)

    # 4. Remove Markdown images ![alt](url)
    text = re.sub(r'!\[.*?\]\(.*?\)', ' ', text)

    # 5. Extract link text from markdown links [anchor](url) -> anchor
    text = re.sub(r'\[(.*?)\]\([^)]*\)', r'\1', text)

    # 6. Remove URLs
    text = re.sub(r'https?://\S+', ' ', text)
    text = re.sub(r'ftp://\S+', ' ', text)
    text = re.sub(r'www\.\S+', ' ', text)

    # 7. Remove all HTML tags
    text = re.sub(r'<[^>]+>', ' ', text)

    # 8. Remove markdown headers (#), quotes (>), list symbols (*, -, +)
    text = re.sub(r'^[#>\-\*\+\d+\.]+\s+', ' ', text, flags=re.MULTILINE)

    # 9. Remove markdown table markers (| --- |)
    text = re.sub(r'\|', ' ', text)

    # 10. Remove inline formatting (*, _, `, ~, ^)
    text = re.sub(r'[`*_~^]', ' ', text)

    # 11. Remove anchor references like {#header-id}
    text = re.sub(r'\{#[^}]+\}', ' ', text)

    return text

def is_valid_word(token: str) -> bool:
    # Filter out empty tokens
    if not token or len(token) > 40:
        return False

    # Check clean token without trailing punctuation
    clean = token.strip(".,;:!?()[]{}\"'“”‘’—–-`")
    if not clean:
        return False

    # Exclude long hexadecimal strings or hashes
    if re.fullmatch(r'[0-9a-fA-F]{16,}', clean):
        return False

    # Exclude pure number sequences longer than 5 digits
    if re.fullmatch(r'\d{6,}', clean):
        return False

    # Must contain at least one letter (Latin or Cyrillic)
    if not re.search(r'[a-zA-Zа-яА-ЯёЁ]', clean):
        return False

    # Reject file paths or weird shell snippets
    if '/' in clean or '\\' in clean or '=' in clean or '{' in clean or '}' in clean:
        return False

    return True

def process_file(file_path: Path) -> list:
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            raw_text = f.read()
    except Exception:
        return []

    cleaned = sanitize_content(raw_text)
    raw_words = cleaned.split()
    valid_words = []

    for w in raw_words:
        w_stripped = w.strip("()[]{}\"'“”‘’`_")
        if is_valid_word(w_stripped):
            # Normalize trailing punctuation (keep single period, comma, question, or exclamation for musical cadence)
            # Remove repeated punctuation
            clean_word = re.sub(r'([.,;:!?])+', r'\1', w_stripped)
            valid_words.append(clean_word)

    return valid_words

def build_corpus(raw_dir: str, output_path: str):
    raw_path = Path(raw_dir)
    if not raw_path.exists():
        print(f"Error: Directory {raw_dir} does not exist.")
        sys.exit(1)

    print(f"Scanning markdown and text files in {raw_dir}...")
    target_files = []
    for root, dirs, files in os.walk(raw_path):
        # Skip git metadata
        if '.git' in dirs:
            dirs.remove('.git')
        for file in files:
            ext = Path(file).suffix.lower()
            if ext in ('.md', '.markdown', '.txt'):
                target_files.append(Path(root) / file)

    print(f"Found {len(target_files)} documentation files to process.")

    total_words = 0
    unique_words = set()
    output_file = Path(output_path)
    output_file.parent.mkdir(parents=True, exist_ok=True)

    with open(output_file, 'w', encoding='utf-8') as out:
        for idx, fpath in enumerate(target_files, 1):
            words = process_file(fpath)
            if not words:
                continue
            for w in words:
                out.write(w + '\n')
                unique_words.add(w.lower().rstrip(".,;:!?"))
                total_words += 1

    file_size_mb = output_file.stat().st_size / (1024 * 1024)
    print("\n--- Corpus Generation Complete ---")
    print(f"Total Words Extracted: {total_words:,}")
    print(f"Unique Vocabulary:     {len(unique_words):,}")
    print(f"Corpus File:           {output_file} ({file_size_mb:.2f} MB)")
    print("----------------------------------")

if __name__ == '__main__':
    base_dir = Path(__file__).resolve().parent
    raw_directory = base_dir / 'data' / 'raw'
    out_corpus = base_dir / 'data' / 'corpus.txt'
    build_corpus(str(raw_directory), str(out_corpus))
