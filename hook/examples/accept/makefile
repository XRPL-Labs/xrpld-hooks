all:
	wasmcc accept.c -o /tmp/accept.wasm -O0 -Wl,--allow-undefined -I../
	wasm-opt -O2 /tmp/accept.wasm -o accept.wasm
	hook-cleaner accept.wasm

