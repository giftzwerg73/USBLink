#!/bin/bash

echo ""
echo run_clang-formt.sh V1.0
echo ""
echo run clang-format ...
echo ""
for dir in *; do
    if [[ -f $dir ]]; then
        if [[ ( "${dir: -2}"  == *.c ) || ( "${dir: -2}"  == *.h ) ]]; then
            if [[ ( "$dir" != "" ) ]]; then
                # echo $dir
                clang-format --verbose -i --Werror --style=file $dir
            else
                echo Skipping $dir ...
            fi
        fi
    fi
done
echo "" 

