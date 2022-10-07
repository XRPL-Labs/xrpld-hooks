#!/bin/bash
ERRORS="0"
cat SetHook_wasm.h  | tr -d '\n' | grep -Po '{[0-9A-FUx, ]*}' | tr -d ' ,{}U' | sed -E 's/0x//g' | 
    while read -r line
    do
        xxd -r -p <<< $line | wasm-objdump -d -
        if [ "$?" -gt "0" ]
        then
            ERRORS=`echo $ERRORS + 1 | bc`
        fi
    done
echo "Errors decompiling: $ERRORS"
