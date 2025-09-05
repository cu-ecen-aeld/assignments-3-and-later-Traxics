#!/bin/bash

# Check if the number of arguments is correct
if [ "$#" -ne 2 ]; then
    echo "Error: Invalid number of arguments."
    echo "Usage: write.sh <directory-path-to-file> <string>"
    exit 1
fi

writefile=$1
writestr=$2

# Create the parent directory if it doesn't exist
mkdir -p "$(dirname "$writefile")"

# Try to write to the file, and check for errors
if ! echo "$writestr" > "$writefile"; then
    echo "Error: Failed to create or write to file"
    exit 1
fi

echo "File created and written successfully"
exit 0
