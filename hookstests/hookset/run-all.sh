#!/bin/bash
RESULT=""
FAILCOUNT=0
PASSCOUNT=0
for i in `ls test-*.js`; do
    echo Running $i
    node $i 2>/dev/null >/dev/null;
    if [ "$?" -eq "0" ];
    then       
        RESULT=`echo $RESULT'~'$i' -- PASS'`
        PASSCOUNT="`echo $PASSCOUNT + 1 | bc`"
    else
        RESULT=`echo $RESULT'~'$i' -- FAIL'`
        FAILCOUNT="`echo $FAILCOUNT + 1 | bc`"
    fi
done
echo
echo "Results:"
RESULT=$RESULT~
echo Passed: $PASSCOUNT, Failed: $FAILCOUNT, Total: `echo $PASSCOUNT + $FAILCOUNT | bc`
echo $RESULT | sed 's/.js//g' | tr '~' '\n'
