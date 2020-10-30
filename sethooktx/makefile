all: x.wasm accept.wasm reject.wasm rollback.wasm print.wasm print2.wasm generateSF carbon.wasm recursiveloop.wasm liteacc.wasm

clean:
	rm *.wasm generateSF


hex2prefill : hex2prefill.c
	gcc utils/hex2prefill.c -o utils/hex2prefill

sfcodes.h: generateSF
	./generateSF > sfcodes.h

generateSF: generateSF.c
	gcc generateSF.c -o generateSF

x.wasm: x.c sfcodes.h hookapi.h hookmacro.h
	wasmcc x.c -o x.wasm  -Wl,--allow-undefined 

accept.wasm: accept.c sfcodes.h hookapi.h hookmacro.h
	wasmcc accept.c -o accept.wasm -Wl,--allow-undefined 

reject.wasm: reject.c sfcodes.h hookapi.h hookmacro.h
	wasmcc reject.c -o reject.wasm -Wl,--allow-undefined 

rollback.wasm: rollback.c sfcodes.h hookapi.h hookmacro.h
	wasmcc rollback.c -o rollback.wasm -Wl,--allow-undefined 

print.wasm: print.c sfcodes.h hookapi.h hookmacro.h
	wasmcc print.c -o print.wasm -Wl,--allow-undefined 

print2.wasm: print2.c sfcodes.h hookapi.h hookmacro.h
	wasmcc print2.c -o print2.wasm -Wl,--allow-undefined 

carbon.wasm: carbon.c sfcodes.h hookapi.h hookmacro.h
	wasmcc carbon.c -o carbon.wasm -Wl,--allow-undefined 

recursiveloop.wasm: recursiveloop.c hookapi.h hookmacro.h
	wasmcc recursiveloop.c -o recursiveloop.wasm  -Wl,--allow-undefined 

liteacc.wasm: liteacc.c sfcodes.h hookapi.h hookmacro.h
	wasmcc liteacc.c -o liteacc.wasm -Wl,--allow-undefined 
