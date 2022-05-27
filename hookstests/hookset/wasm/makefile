all: accept.wasm makestate.wasm makestate2.wasm checkstate.wasm rollback.wasm aaw.wasm rmstate.wasm
accept.wasm: accept.c
	wasmcc accept.c -o accept.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner accept.wasm
rollback.wasm: rollback.c
	wasmcc rollback.c -o rollback.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner rollback.wasm
makestate.wasm: makestate.c
	wasmcc makestate.c -o makestate.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner makestate.wasm
makestate2.wasm: makestate2.c
	wasmcc makestate2.c -o makestate2.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner makestate2.wasm
checkstate.wasm: checkstate.c
	wasmcc checkstate.c -o checkstate.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner checkstate.wasm
rmstate.wasm: rmstate.c
	wasmcc rmstate.c -o rmstate.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner rmstate.wasm
aaw.wasm: aaw.c
	wasmcc aaw.c -o aaw.wasm -O0 -Wl,--allow-undefined -I../
	hook-cleaner aaw.wasm
