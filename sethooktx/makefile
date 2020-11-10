all: x.wasm accept.wasm reject.wasm rollback.wasm print.wasm print2.wasm generateSF carbon.wasm recursiveloop.wasm liteacc.wasm

clean:
	rm *.wasm generateSF

sfcodes.h: generateSF
	./generateSF > sfcodes.h

generateSF: generateSF.c
	gcc generateSF.c -o generateSF

accept.wasm: accept.c sfcodes.h hookapi.h hookmacro.h
	wasmcc accept.c -o accept.wasm -Wl,--allow-undefined 

reject.wasm: reject.c sfcodes.h hookapi.h hookmacro.h
	wasmcc reject.c -o reject.wasm -Wl,--allow-undefined 

rollback.wasm: rollback.c sfcodes.h hookapi.h hookmacro.h
	wasmcc rollback.c -o rollback.wasm -Wl,--allow-undefined 

carbon.wasm: carbon.c sfcodes.h hookapi.h hookmacro.h
	wasmcc carbon.c -o carbon.wasm -Wl,--allow-undefined 

recursiveloop.wasm: recursiveloop.c hookapi.h hookmacro.h
	wasmcc recursiveloop.c -o recursiveloop.wasm  -Wl,--allow-undefined 

liteacc.wasm: liteacc.c sfcodes.h hookapi.h hookmacro.h
	wasmcc liteacc.c -o liteacc.wasm -Oz -Wl,--allow-undefined 

firewall.wasm: firewall.c sfcodes.h hookapi.h hookmacro.h
	wasmcc firewall.c -o firewall.wasm -Oz -Wl,--allow-undefined

blacklist.wasm: blacklist.c sfcodes.h hookapi.h hookmacro.h
	wasmcc blacklist.c -o blacklist.wasm -O0 -Wl,--allow-undefined
