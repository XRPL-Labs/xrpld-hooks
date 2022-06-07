all:
	wasmcc notary.c -o notary.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner notary.wasm

