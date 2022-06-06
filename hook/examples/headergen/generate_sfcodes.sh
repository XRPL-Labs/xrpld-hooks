#/bin/bash
RIPPLED_ROOT="`git rev-parse --show-toplevel`/src/ripple"
echo '// For documentation please see: https://xrpl-hooks.readme.io/reference/'
echo '// Generated using generate_sfcodes.sh'
cat $RIPPLED_ROOT/protocol/impl/SField.cpp | grep -E '^CONSTRUCT_' |
    sed 's/UINT16/1/g' |
    sed 's/UINT32/2/g' |
    sed 's/UINT64/3/g' |
    sed 's/HASH128/4/g' |
    sed 's/HASH256/5/g' |
    sed 's/UINT128/4/g' |
    sed 's/UINT256/5/g' |
    sed 's/AMOUNT/6/g' |
    sed 's/VL/7/g' |
    sed 's/ACCOUNT/8/g' |
    sed 's/OBJECT/14/g' |
    sed 's/ARRAY/15/g' |
    sed 's/UINT8/16/g' |
    sed 's/HASH160/17/g' |
    sed 's/UINT160/17/g' |
    sed 's/PATHSET/18/g' |
    sed 's/VECTOR256/19/g' |
    sed 's/UINT96/20/g' |
    sed 's/UINT192/21/g' |
    sed 's/UINT384/22/g' |
    sed 's/UINT512/23/g' |
    grep -Eo '"([^"]+)", *([0-9]+), *([0-9]+)' |
    sed 's/"//g' | sed 's/ *//g' | sed 's/,/ /g' |
    awk '{print ("#define sf"$1" (("$2"U << 16U) + "$3"U)")}'

