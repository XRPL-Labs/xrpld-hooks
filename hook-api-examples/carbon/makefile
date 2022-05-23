all:
	wasmcc carbon.c -o carbon.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner carbon.wasm


