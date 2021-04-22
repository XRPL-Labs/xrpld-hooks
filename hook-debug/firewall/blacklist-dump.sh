#!/bin/bash
cd ..
./rippled account_objects rNsA4VzfZZydhGAvfHX3gdpcQMMoJafd6v | sed 's/CreateCode.*/CreateCode": <hook wasm>,/g' |
    sed 's/"HookStateKey" : "0000000000000000000000000000000000000000000000000000000000000000"/\0 <last seq key>/g' |
    sed 's/"HookStateData" : "01"/\0 <blacklist entry>/g'
