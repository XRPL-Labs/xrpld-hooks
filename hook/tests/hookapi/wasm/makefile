all: log.wasm root.wasm
log.wasm: log.c
	wasmcc log.c -o log.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner log.wasm
root.wasm: root.c
	wasmcc root.c -o root.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner root.wasm
