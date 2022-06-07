all:
	wasmcc notary.c -o /tmp/notary.wasm -O0 -Wl,--allow-undefined -I../
	wasm-opt -O2 /tmp/notary.wasm -o notary.wasm
	hook-cleaner notary.wasm

