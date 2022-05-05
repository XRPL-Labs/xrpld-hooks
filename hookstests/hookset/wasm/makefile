all: accept.wasm makestate.wasm makestate2.wasm checkstate.wasm
accept.wasm: accept.c
	wasmcc accept.c -o accept.wasm -O0 -Wl,--allow-undefined -I../
makestate.wasm: makestate.c
	wasmcc makestate.c -o makestate.wasm -O0 -Wl,--allow-undefined -I../
makestate2.wasm: makestate2.c
	wasmcc makestate2.c -o makestate2.wasm -O0 -Wl,--allow-undefined -I../
checkstate.wasm: checkstate.c
	wasmcc checkstate.c -o checkstate.wasm -O0 -Wl,--allow-undefined -I../

