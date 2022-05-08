#!/bin/bash

declare -A tlds

for arg in $*
do
    tlds[$arg]=0
done

for file in `find ./output -name "*.out"`
do
    while read -r line; do
        link=$(echo $line | cut -f 1 -d " ")
        tld="${link##*.}"
        occurences=$(echo $line | cut -f 2 -d " ")
        if [ ! -z "$tld" ]; then if [ ${tlds[$tld]+_} ]; then ((tlds[$tld]+=occurences)); fi; fi
    done <$file 
done

for arg in $*
do
    echo $arg ${tlds[$arg]}
done
