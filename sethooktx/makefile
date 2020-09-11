all:
	wasmcc x.c -o x.wasm -Wl,--allow-undefined 
	wasmcc accept.c -o accept.wasm -Wl,--allow-undefined 
	wasmcc reject.c -o reject.wasm -Wl,--allow-undefined 
	wasmcc rollback.c -o rollback.wasm -Wl,--allow-undefined 
	wasmcc print.c -o print.wasm -Wl,--allow-undefined 
