# Hooks Public Testnet Quickstart
## What's in this docker?
1. An `rippled` instance connected to the Hooks Public Testnet.
2. A compiler toolchain for building hooks (wasmcc, wasm2wat, etc...)
3. Example hooks and support scripts to install them on your account.

## Get testnet XRP
The Faucet is on the (main page)[https://hooks-testnet.xrpl-labs.com/]. Make a note of your secret (family seed) because you will need it for all the examples.

## Testnet explorer
Use the [Testnet Explorer](https://hooks-testnet-explorer.xrpl-labs.com/) to view transactions, accounts and hook state objects as you go.

## File types
1. The example hooks are written in C. Any file ending in .c is a hook. Any file ending in .h is a header file used by
all hooks.
2. Hooks compile to `.wasm` files. These are hook binaries ready to be installed onto an account on the ledger using
the `SetHook` transaction.
3. Javascript files `.js` are runnable with nodejs and provide a way to installing hooks and interacting with the ledger through ripple-lib.

## Building
To build you can run `make` from any hook's directory. The example `makefile` in each directory shows you how to build a hook.

## Interacting
To interact with the example hooks change (`cd`) into one of these directories:
- liteacc
- firewall
- carbon
- accept
- notary
- peggy
    
All example hooks are installed by running `node <hook name>.js`. The usage information will be provided at the commandline.

You can check a reported `SetHook` transaction ID with the [Hooks Testnet Explorer](https://hooks-testnet-explorer.xrpl-labs.com/).

Finally run the additional `.js` files to interact with the Hook or write your own interaction.

## Viewing state
You can view the current Hook State for your Hook by locating the account it is installed on with the (Hooks Testnet Explorer)[https://hooks-testnet-explorer.xrpl-labs.com/].

You can also run `./rippled account_objects <account on which the hook is installed on>` to inspect the Hook's State Data. 

## Minimum example hook
Please have a look at the accept hook in `./accept/`

## Tracing output
Output is written to a file in the current working directory called `log`.

Rippled produces a large amount of output in `trace` mode (which is the default mode used in this example). You may
find that you need to scroll back considerably after running a hook or interacting with a hook to find out what
precisely happened.

Greping for the relevant account will help, as all Hooks prepend the otxn account and hook account to the trace line.

