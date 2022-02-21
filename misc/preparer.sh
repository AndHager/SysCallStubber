#!/bin/bash

cp -r ./out ./tmp
for file in ./tmp/* ; do
  if [[ "${file: -11}" == "_output.txt" ]]
  then # file is Executable
    echo "__INFO__ Executing: sed -i -n '/^[^#]/!p' $file"
    sed -i -n '/^[^#]/!p' $file
  else
    echo "__INFO__ Executing: rm -rf $file"
    rm -rf $file
  fi
done
zip -r out_$1.zip ./tmp
rm -rf ./tmp
