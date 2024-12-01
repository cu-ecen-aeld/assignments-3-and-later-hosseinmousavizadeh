#!/bin/bash

filesdir=$1

searchstr=$2

if (($# < 2));
then
    echo "Too few arguments"
    exit 1
elif [ ! -d $filesdir ]; 
then
    echo "Not a directory"
    exit 1
fi

number_of_files=$(($(find $filesdir| wc -l)-1))

number_of_matches=$(grep -r $searchstr $filesdir| wc -l)

echo "The number of files are $number_of_files and the number of matching lines are $number_of_matches"
