#!/bin/bash
ERRORS="0"
COUNTER="0"
cat SetHook_wasm.h  | tr -d '\n' | grep -Po '{[0-9A-FUx, ]*}' | tr -d ' ,{}U' | sed -E 's/0x//g' | 
    while read -r line
    do
        echo ""
        echo "======== WASM: $COUNTER ========="
        xxd -r -p <<< $line | wasm-objdump -d -
        if [ "$?" -gt "0" ]
        then
            ERRORS=`echo $ERRORS + 1 | bc`
        fi
        COUNTER=`echo $COUNTER+1|bc`
    done
echo "Errors decompiling: $ERRORS"
