all:
	wasmcc doubler.c -o doubler.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner doubler.wasm

