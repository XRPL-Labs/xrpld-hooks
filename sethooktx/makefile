all: sfcodes.h generateSF accept.wasm reject.wasm rollback.wasm carbon.wasm rollback.wasm liteacc.wasm firewall.wasm blacklist.wasm

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

liteacc.wasm: liteacc.c sfcodes.h hookapi.h hookmacro.h
	wasmcc liteacc.c -o liteacc.wasm -Oz -Wl,--allow-undefined

firewall.wasm: firewall.c sfcodes.h hookapi.h hookmacro.h
	wasmcc firewall.c -o firewall.wasm -Oz -Wl,--allow-undefined

blacklist.wasm: blacklist.c sfcodes.h hookapi.h hookmacro.h
	wasmcc blacklist.c -o blacklist.wasm -O0 -Wl,--allow-undefined

#ascarbon.wasm: ascarbon.ts
#	/bin/cp ascarbon.ts env.ts
#	asc env.ts -o ascarbon.wasm --runtime none --lowMemoryLimit --initialMemory 1 --maximumMemory 1 -O3z --converge
#	/bin/rm -f env.ts
