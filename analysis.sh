#!/bin/bash

# check if jq is installed
if ! [ -x "$(command -v jq)" ]; then
    echo 'Error: jq is not installed.' >&2
    exit 1
fi

# for each folder in results/raw/
# get the json value for key "name" and "binaries[0].targets" in file jobFile.json

for folder in results/raw/*; do
    echo ">>>>"
    echo "Processing $folder"
    jq '.name' $folder/jobFile.json
    jq '.binaries[0].targets' $folder/jobFile.json
    python3 discovery.py $folder/job.log --testbed
    python3 energest-stats.py $folder/job.log --testbed
done
