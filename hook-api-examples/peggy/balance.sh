#!/bin/bash
HOOKDATA=`(cd ../../build; ./rippled account_objects rECE33X6yXqM7MpjXCqG8nsdSWtSFzeGrS 2> /dev/null | grep '"HookStateData"' | awk '{print $3}' | sed -E 's/[^A-F0-9]//g')`
FLOAT1=`echo $HOOKDATA | cut -c1-16`
FLOAT2=`echo $HOOKDATA | cut -c17-32`
echo $FLOAT1
echo $FLOAT2
echo "PUSD: `node xls17.js 0x$FLOAT1`, XRP: `node xls17.js 0x$FLOAT2`"
