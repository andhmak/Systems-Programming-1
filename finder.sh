#!/bin/bash

# Declare associative array
declare -A tlds

# Initialise it
for arg in $*
do
    tlds[$arg]=0
done

# For every output file
for file in `find ./output -name "*.out"`
do
    # For every line
    while read -r line; do
        # Increase the TLD occurences if among the TLD's we're searching
        link=$(echo $line | cut -f 1 -d " ")
        tld="${link##*.}"
        occurences=$(echo $line | cut -f 2 -d " ")
        if [ ! -z "$tld" ]; then if [ ${tlds[$tld]+_} ]; then ((tlds[$tld]+=occurences)); fi; fi
    done <$file 
done

# Print the result
for arg in $*
do
    echo $arg ${tlds[$arg]}
done
