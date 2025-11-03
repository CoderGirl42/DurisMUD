#!/bin/bash
# Arih: Convert ALL remaining sprintf to snprintf - 20251103
# This handles all sprintf patterns including pointer arithmetic

# Simple cases: sprintf(buffer, ...)
# Convert to: snprintf(buffer, sizeof(buffer), ...)
perl -i -pe 's/\bsprintf\s*\(\s*(\w+)\s*,/snprintf($1, MAX_STRING_LENGTH,/g' *.c

# Pointer arithmetic cases: sprintf(buf + strlen(buf), ...)
# Convert to: snprintf(buf + strlen(buf), MAX_STRING_LENGTH - strlen(buf),/...)
# This is trickier - for now, use a conservative approach
perl -i -pe 's/\bsprintf\s*\(\s*(\w+)\s*\+\s*strlen\s*\(\s*\1\s*\)\s*,/snprintf($1 + strlen($1), MAX_STRING_LENGTH - strlen($1),/g' *.c

# Other expression cases - use MAX_STRING_LENGTH
perl -i -pe 's/\bsprintf\s*\(/snprintf_PLACEHOLDER(/g' *.c
perl -i -pe 's/snprintf_PLACEHOLDER\(\s*([^,]+)\s*,/snprintf($1, MAX_STRING_LENGTH,/g' *.c

echo "Done converting sprintf to snprintf"
