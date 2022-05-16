all: accept.wasm makestate.wasm makestate2.wasm checkstate.wasm rollback.wasm aaw.wasm
accept.wasm: accept.c
	wasmcc accept.c -o accept.wasm -O0 -Wl,--allow-undefined -I../
	/root/hook-cleaner-c/cleaner accept.wasm
rollback.wasm: rollback.c
	wasmcc rollback.c -o rollback.wasm -O0 -Wl,--allow-undefined -I../
	/root/hook-cleaner-c/cleaner rollback.wasm
makestate.wasm: makestate.c
	wasmcc makestate.c -o makestate.wasm -O0 -Wl,--allow-undefined -I../
	/root/hook-cleaner-c/cleaner makestate.wasm
makestate2.wasm: makestate2.c
	wasmcc makestate2.c -o makestate2.wasm -O0 -Wl,--allow-undefined -I../
	/root/hook-cleaner-c/cleaner makestate2.wasm
checkstate.wasm: checkstate.c
	wasmcc checkstate.c -o checkstate.wasm -O0 -Wl,--allow-undefined -I../
	/root/hook-cleaner-c/cleaner checkstate.wasm
aaw.wasm: aaw.c
	wasmcc aaw.c -o aaw.wasm -O0 -Wl,--allow-undefined -I../
	/root/hook-cleaner-c/cleaner aaw.wasm
