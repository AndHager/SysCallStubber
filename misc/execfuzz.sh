#!/bin/bash

cd out
pids=()
for file in fuzz-*; do
    if [[ -x "$file" ]]
    then # file is Executable
        echo "__INFO__: processing $file."
	( ./$file & ) &> ./${file}_output.txt # create process for each fuzzer
	pids+=$! # get pid of the last process
    fi
done

sleep 900

for pid in "${pids[*]}"; do
    let p=pid
    kill -KILL $p
done

