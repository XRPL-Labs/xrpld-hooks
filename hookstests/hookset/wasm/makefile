all: accept.wasm makestate.wasm checkstate.wasm
accept.wasm: accept.c
	wasmcc accept.c -o accept.wasm -O0 -Wl,--allow-undefined -I../
makestate.wasm: makestate.c
	wasmcc makestate.c -o makestate.wasm -O0 -Wl,--allow-undefined -I../
checkstate.wasm: checkstate.c
	wasmcc checkstate.c -o checkstate.wasm -O0 -Wl,--allow-undefined -I../

