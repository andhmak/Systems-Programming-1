
#!/bin/bash


declare -A tlds

for arg in $*
do
    tlds[$arg]=0
done

echo ${tlds["a"]}
