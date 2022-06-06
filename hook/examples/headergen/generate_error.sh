#!/bin/bash
RIPPLED_ROOT="`git rev-parse --show-toplevel`/src/ripple"
echo '// For documentation please see: https://xrpl-hooks.readme.io/reference/'
echo '// Generated using generate_error.sh'
echo '#ifndef HOOK_ERROR_CODES'
cat $RIPPLED_ROOT/app/hook/Enum.h | tr -d '\n' | grep -Eo 'hook_return_code : int64_t *{[^}]+}' | grep -Eo '[A-Z_]+ *= *[0-9-]+' | sed -E 's/ *= */ /g' | sed -E 's/^/#define /g'
echo '#define HOOK_ERROR_CODES'
echo '#endif //HOOK_ERROR_CODES'
