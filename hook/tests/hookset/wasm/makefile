all: accept.wasm makestate.wasm makestate2.wasm checkstate.wasm rollback.wasm aaw.wasm rmstate.wasm ginv.wasm multiguard.wasm
accept.wasm: accept.c
	wasmcc accept.c -o accept.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner accept.wasm
rollback.wasm: rollback.c
	wasmcc rollback.c -o rollback.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner rollback.wasm
makestate.wasm: makestate.c
	wasmcc makestate.c -o makestate.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner makestate.wasm
makestate2.wasm: makestate2.c
	wasmcc makestate2.c -o makestate2.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner makestate2.wasm
checkstate.wasm: checkstate.c
	wasmcc checkstate.c -o checkstate.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner checkstate.wasm
rmstate.wasm: rmstate.c
	wasmcc rmstate.c -o rmstate.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner rmstate.wasm
aaw.wasm: aaw.c
	wasmcc aaw.c -o aaw.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner aaw.wasm
ginv.wasm: ginv.c
	wasmcc ginv.c -o ginv.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner ginv.wasm
multiguard.wasm: multiguard.c
	wasmcc multiguard.c -o multiguard.wasm -O2 -Wl,--allow-undefined -I../
	hook-cleaner multiguard.wasm



