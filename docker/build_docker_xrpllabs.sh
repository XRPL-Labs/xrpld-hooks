#!/bin/bash
cp -r ../hook-api-examples docker/js #docker doesnt like symlinks?
/usr/bin/cp /root/wabt/bin/wasm2wat docker/ 
docker build --tag xrpllabsofficial/xrpld-hooks-testnet:latest . && docker create xrpllabsofficial/xrpld-hooks-testnet
rm -rf docker/js
docker push xrpllabsofficial/xrpld-hooks-testnet:latest
