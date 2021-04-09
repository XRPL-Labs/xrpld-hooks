# Hooks Public Testnet
This is a fork of the rippled codebase incorporating the work-in-progress "Hooks" amendment. This amendment will allow web assembly smart contracts to run directly on the XRP ledger when completed and adopted.

## Docker Container
Building `rippled` can be non-trivial, especially in this case since modified libraries are used. We have provided a testnet docker container for your convenience. This container contains an instance of `rippled` configured as a "Hooks stock node" running on the Public Testnet. You can interact with it using the steps below:
### Starting the container
1. Download and install docker.
2. To download the container use:
```bash
docker pull richardah/xrpld-hooks-testnet
```
3. Then to run the container interactively use:
```bash
docker run --name xrpld-hooks richardah/xrpld-hooks-testnet &
docker exec -it xrpld-hooks /bin/bash
```
4. Set up a second terminal to view the log:

Open a new terminal window on your system and run.
```bash
docker exec -it xrpld-hooks tail -f log
```
 This will show you the trace log of xrpld as it runs, which will be important for knowing if your transactions fail or succeed and what actions the hooks take.

5. If you need to kill the container and restart it:
```bash
exit #from the container
docker kill xrpld-hooks
docker container prune -f
```
 Then repeat step 3.

### Interacting with the container
After following the above steps you will be inside a shell inside the container. Rippled will already be running with the correct settings. Read the README.md in the container for further instructions on installing and interacting with the example hooks.
                                                                                                                      
## What's in this docker?
1. A `rippled` instance connected to the Hooks Public Testnet.
2. A compiler toolchain for building hooks (wasmcc, wasm2wat, etc...)
3. Example hooks and support scripts to install them on your account.

## Get testnet XRP
The Faucet is on the [main page](https://hooks-testnet.xrpl-labs.com/). Make a note of your secret (family seed) because you will need it for all the examples.

## Testnet explorer
Use the [Testnet Explorer](https://hooks-testnet-explorer.xrpl-labs.com/) to view transactions, accounts and hook state objects as you go.

## File types
1. The example hooks are written in C. Any file ending in `.c` is a hook. Any file ending in `.h` is a header file used by
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

## Hook API
- Documentation for the Hook API can be found at the [Hooks Testnet Site](https://hooks-testnet.xrpl-labs.com/).
- For further details check:
1. `hook-api-examples/hookapi.h`
2. `src/ripple/app/tx/applyHook.h`
3. `src/ripple/app/tx/impl/applyHook.cpp`

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

## SetHook Transaction
Set a Hook on an activated account using a SetHook Transaction (ttHOOK_SET = 22). This must contain the following fields:
- sfAccount
- sfCreateCode: Containing the binary of the web assembly
- sfHookOn: An unsigned 64bit integer (explained bellow)

### sfHookOn
Each bit in this unsigned int64 indicates whether the Hook should execute on a particular transaction type. All bits are *active low* **except** bit 22 which is *active high*. Since 22 is ttHOOK_SET this means the default value of all 0's will not fire on a SetHook transaction but will fire on every other transaction type. This is a deliberate design choice to help people avoid bricking their XRPL account with a misbehaving hook.

Bits are numbered from right to left from 0 to 63).

Examples:

1. If we want to completely disable the hook:
```C
~(1ULL << 22) /* every bit is 1 except bit 22 which is 0 */
```

2. If we want to disable the hook on everything except ttPAYMENT:
```C
~(1ULL << 22) & ~(1ULL)
```

3. If we want to enable the hook on everything except ttHOOK_SET
```C
0
```

4. If we want to enable hook firing on ttHOOK_SET (dangerous) and every other transaction type:
```C
(1ULL << 22)
```



-------

# The XRP Ledger

The XRP Ledger is a decentralized cryptographic ledger powered by a network of peer-to-peer servers. The XRP Ledger uses a novel Byzantine Fault Tolerant consensus algorithm to settle and record transactions in a secure distributed database without a central operator.

## XRP
XRP is a public, counterparty-free asset native to the XRP Ledger, and is designed to bridge the many different currencies in use worldwide. XRP is traded on the open-market and is available for anyone to access. The XRP Ledger was created in 2012 with a finite supply of 100 billion units of XRP. Its creators gifted 80 billion XRP to a company, now called [Ripple](https://ripple.com/), to develop the XRP Ledger and its ecosystem. Ripple uses XRP to help build the Internet of Value, ushering in a world in which money moves as fast and efficiently as information does today.

## rippled
The server software that powers the XRP Ledger is called `rippled` and is available in this repository under the permissive [ISC open-source license](LICENSE). The `rippled` server is written primarily in C++ and runs on a variety of platforms.

### Build from Source

* [Linux](Builds/linux/README.md)
* [Mac](Builds/macos/README.md)
* [Windows](Builds/VisualStudio2017/README.md)

## Key Features of the XRP Ledger

- **[Censorship-Resistant Transaction Processing][]:** No single party decides which transactions succeed or fail, and no one can "roll back" a transaction after it completes. As long as those who choose to participate in the network keep it healthy, they can settle transactions in seconds.
- **[Fast, Efficient Consensus Algorithm][]:** The XRP Ledger's consensus algorithm settles transactions in 4 to 5 seconds, processing at a throughput of up to 1500 transactions per second. These properties put XRP at least an order of magnitude ahead of other top digital assets.
- **[Finite XRP Supply][]:** When the XRP Ledger began, 100 billion XRP were created, and no more XRP will ever be created. The available supply of XRP decreases slowly over time as small amounts are destroyed to pay transaction costs.
- **[Responsible Software Governance][]:** A team of full-time, world-class developers at Ripple maintain and continually improve the XRP Ledger's underlying software with contributions from the open-source community. Ripple acts as a steward for the technology and an advocate for its interests, and builds constructive relationships with governments and financial institutions worldwide.
- **[Secure, Adaptable Cryptography][]:** The XRP Ledger relies on industry standard digital signature systems like ECDSA (the same scheme used by Bitcoin) but also supports modern, efficient algorithms like Ed25519. The extensible nature of the XRP Ledger's software makes it possible to add and disable algorithms as the state of the art in cryptography advances.
- **[Modern Features for Smart Contracts][]:** Features like Escrow, Checks, and Payment Channels support cutting-edge financial applications including the [Interledger Protocol](https://interledger.org/). This toolbox of advanced features comes with safety features like a process for amending the network and separate checks against invariant constraints.
- **[On-Ledger Decentralized Exchange][]:** In addition to all the features that make XRP useful on its own, the XRP Ledger also has a fully-functional accounting system for tracking and trading obligations denominated in any way users want, and an exchange built into the protocol. The XRP Ledger can settle long, cross-currency payment paths and exchanges of multiple currencies in atomic transactions, bridging gaps of trust with XRP.

[Censorship-Resistant Transaction Processing]: https://developers.ripple.com/xrp-ledger-overview.html#censorship-resistant-transaction-processing
[Fast, Efficient Consensus Algorithm]: https://developers.ripple.com/xrp-ledger-overview.html#fast-efficient-consensus-algorithm
[Finite XRP Supply]: https://developers.ripple.com/xrp-ledger-overview.html#finite-xrp-supply
[Responsible Software Governance]: https://developers.ripple.com/xrp-ledger-overview.html#responsible-software-governance
[Secure, Adaptable Cryptography]: https://developers.ripple.com/xrp-ledger-overview.html#secure-adaptable-cryptography
[Modern Features for Smart Contracts]: https://developers.ripple.com/xrp-ledger-overview.html#modern-features-for-smart-contracts
[On-Ledger Decentralized Exchange]: https://developers.ripple.com/xrp-ledger-overview.html#on-ledger-decentralized-exchange


## Source Code
[![travis-ci.com: Build Status](https://travis-ci.com/ripple/rippled.svg?branch=develop)](https://travis-ci.com/ripple/rippled)
[![codecov.io: Code Coverage](https://codecov.io/gh/ripple/rippled/branch/develop/graph/badge.svg)](https://codecov.io/gh/ripple/rippled)

### Repository Contents

| Folder     | Contents                                         |
|:-----------|:-------------------------------------------------|
| `./bin`    | Scripts and data files for Ripple integrators.   |
| `./Builds` | Platform-specific guides for building `rippled`. |
| `./docs`   | Source documentation files and doxygen config.   |
| `./cfg`    | Example configuration files.                     |
| `./src`    | Source code.                                     |

Some of the directories under `src` are external repositories included using
git-subtree. See those directories' README files for more details.


## See Also

* [XRP Ledger Dev Portal](https://developers.ripple.com/)
* [XRP News](https://ripple.com/category/xrp/)
* [Setup and Installation](https://developers.ripple.com/install-rippled.html)
* [Doxygen](https://ripple.github.io/rippled)

To learn about how Ripple is transforming global payments, visit
<https://ripple.com/contact/>.
