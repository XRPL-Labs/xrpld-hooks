all:
	wasmcc carbon.c -o /tmp/carbon.wasm -O0 -Wl,--allow-undefined -I../
	wasm-opt -O2 /tmp/carbon.wasm -o carbon.wasm
	hook-cleaner carbon.wasm

