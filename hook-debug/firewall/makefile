all:
	wasmcc firewall.c -o firewall.wasm -O0 -Wl,--allow-undefined -I../
	wasmcc blacklist.c -o blacklist.wasm -O0 -Wl,--allow-undefined -I../

