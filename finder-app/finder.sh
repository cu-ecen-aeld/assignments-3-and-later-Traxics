#!/bin/bash

# Check if the number of arguments is correct
if [ "$#" -ne 2 ]; then
    echo "Error: Invalid number of arguments."
    echo "Usage: finder.sh <directory-path> <search-string>"
    exit 1
fi

filesdir=$1
searchstr=$2

# Check if the provided directory exists and is a directory
if [ ! -d "$filesdir" ]; then
    echo "Error: Directory $filesdir does not exist."
    exit 1
fi

# Find and count the number of files and occurrences of the search string
num_files=$(find "$filesdir" -type f | wc -l)
num_occurrences=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $num_files and the number of matching lines are $num_occurrences"
