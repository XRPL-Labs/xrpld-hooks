all:
	wasmcc peggy.c -o /tmp/peggy.wasm -O0 -Wl,--allow-undefined -I../
	wasm-opt -O2 /tmp/peggy.wasm -o peggy.wasm
	hook-cleaner peggy.wasm

