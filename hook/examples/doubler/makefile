all:
	wasmcc doubler.c -o /tmp/doubler.wasm -O0 -Wl,--allow-undefined -I../
	wasm-opt -O2 /tmp/doubler.wasm -o doubler.wasm
	hook-cleaner doubler.wasm

