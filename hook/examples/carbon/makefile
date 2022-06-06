all:
	wasmcc carbon.c -o carbon.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner carbon.wasm


