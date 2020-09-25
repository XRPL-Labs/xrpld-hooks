all: x.wasm accept.wasm reject.wasm rollback.wasm print.wasm print2.wasm

x.wasm: x.c
	wasmcc x.c -o x.wasm  -Wl,--allow-undefined 

accept.wasm: accept.c
	wasmcc accept.c -o accept.wasm -Wl,--allow-undefined 

reject.wasm: reject.c
	wasmcc reject.c -o reject.wasm -Wl,--allow-undefined 

rollback.wasm: rollback.c
	wasmcc rollback.c -o rollback.wasm -Wl,--allow-undefined 

print.wasm: print.c
	wasmcc print.c -o print.wasm -Wl,--allow-undefined 

print2.wasm: print2.c
	wasmcc print2.c -o print2.wasm -Wl,--allow-undefined 
