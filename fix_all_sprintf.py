#!/usr/bin/env python3
"""
Arih: Convert ALL remaining sprintf to snprintf - handles all patterns - 20251103
"""

import re
import sys
from pathlib import Path

def fix_sprintf_comprehensive(filepath):
    """Convert all sprintf patterns to snprintf."""
    with open(filepath, 'r', encoding='latin-1', errors='ignore') as f:
        content = f.read()

    original_content = content
    changes = 0

    # Pattern 1: sprintf(buf + strlen(buf), ...)
    # Convert to: snprintf(buf + strlen(buf), MAX_STRING_LENGTH - strlen(buf), ...)
    pattern1 = r'\bsprintf\s*\(\s*(\w+)\s*\+\s*strlen\s*\(\s*\1\s*\)\s*,'
    replacement1 = r'snprintf(\1 + strlen(\1), MAX_STRING_LENGTH - strlen(\1),'
    content, count1 = re.subn(pattern1, replacement1, content)
    changes += count1

    # Pattern 2: sprintf(simple_var, ...)  where simple_var is just a word
    # Convert to: snprintf(simple_var, MAX_STRING_LENGTH, ...)
    pattern2 = r'\bsprintf\s*\(\s*(\w+)\s*,'
    replacement2 = r'snprintf(\1, MAX_STRING_LENGTH,'
    content, count2 = re.subn(pattern2, replacement2, content)
    changes += count2

    # Pattern 3: sprintf(complex_expression, ...) - everything else
    # Convert to: snprintf(complex_expression, MAX_STRING_LENGTH, ...)
    pattern3 = r'\bsprintf\s*\(\s*([^,]+?)\s*,'
    replacement3 = r'snprintf(\1, MAX_STRING_LENGTH,'
    content, count3 = re.subn(pattern3, replacement3, content)
    changes += count3

    if content != original_content:
        with open(filepath, 'w', encoding='latin-1') as f:
            f.write(content)
        print(f"{filepath}: {changes} sprintf -> snprintf conversions")
        return changes
    else:
        return 0

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 fix_all_sprintf.py <file1.c> [file2.c ...]")
        sys.exit(1)

    total_changes = 0
    for filepath in sys.argv[1:]:
        changes = fix_sprintf_comprehensive(filepath)
        total_changes += changes

    print(f"\nTotal: {total_changes} conversions across {len(sys.argv)-1} files")

if __name__ == "__main__":
    main()
