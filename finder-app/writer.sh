#!/bin/bash

if (($# < 2));
then
    echo "Too few arguments"
    exit 1
fi

writeFile=$1
writeStr=$2

dir=$(dirname $writeFile)
mkdir -p $dir
echo $writeStr > $writeFile
