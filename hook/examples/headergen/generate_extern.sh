#!/bin/bash
RIPPLED_ROOT="`git rev-parse --show-toplevel`/src/ripple"
echo '// For documentation please see: https://xrpl-hooks.readme.io/reference/'
echo '// Generated using generate_extern.sh'
echo '#include <stdint.h>'
echo '#ifndef HOOK_EXTERN'
cat $RIPPLED_ROOT/app/hook/applyHook.h  |  tr -d '\n' |  grep -Eo 'DECLARE_HOOK[^\(]+\([^\)]+\)'  |  grep DECLARE_HOOK |  cut -d'(' -f2 |  sed -E 's/_t,/_t/g' |  sed -E 's/  */ /g' | sort | grep -Ev '^$' | sed -E s'/\)/ \)/g' | tr '\t' ' '  | sed -E 's/^([^ ]+ [^ ]+)/\1,/g' | sed -E 's/,,*/,/g' | sed -E 's/\)/\)\n/g' | sort | grep -vE '^$' | sed -E 's/^([^,]+),/\1 (/g' | sed -E 's/\( *\)/(void)/g' | sed -E 's/, *\(/(/g' | sed -E 's/  */ /g' | sed -E 's/ *\( /(/g' | sed -E 's/ \)/)/g' | sed -E 's/\)([^;]?)/);\1/g' | sed -E 's/^int/extern int/g' | sed -E 's/^(extern [^ ]+ )/\1\n/g' | grep -Ev '^,+$' | sed -E 's/\(/\(\n    /g' | sed -E 's/, */,\n    /g' | sed -E 's/^extern/\nextern/g' | sed -E 's/\);/\n);/g' | sed -E 's/^(_g\()/__attribute__((noduplicate))\n\1/g'
echo '#define HOOK_EXTERN'
echo '#endif //HOOK_EXTERN'
