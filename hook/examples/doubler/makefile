all:
	wasmcc doubler.c -o doubler.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner doubler.wasm

