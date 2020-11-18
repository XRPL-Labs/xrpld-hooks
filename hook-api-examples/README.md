# Hooks Technology Preview Quickstart

## File types
1. The example hooks are written in C. Any file ending in .c is a hook. Any file ending in .h is a header file used by
all hooks.
2. Hooks compile to .wasm files. These are hook binaries ready to be installed onto an account on the ledger using
the `SetHook` transaction.
3. Javascript files .js are runnable with nodejs and provide a way to installing hooks and interacting with the ledger
through ripple-lib.

## Interacting
To interact with the example hooks change (`cd`) into one of these directories:
- liteacc
- firewall
- carbon
- accept
    
Then run `node <hook name>.js` to install the hook.

Finally run the additional .js files to interact with the hook or write your own interaction

## Viewing state
Run `./rippled account_objects <account on which the hook is installed on>` to inspect the Hook's State Data.
Liteacc and Firewall have helper scripts to dump this info. Note that the CreateCode entry contains the entire binary
of the hook, so it's best to truncate these entries with sed: e.g. `sed 's/CreateCode.*/CreateCode": <hook wasm>,/g'`

## Building
To build you can run `make` from any hook's directory. The example `makefile` in each directory shows you how to build a hook.

## Minimum example hook
Please have a look at the accept hook in `./accept/`

## Other notes
Rippled produces a large amount of output in `trace` mode (which is the default mode used in this example). You may
find that you need to scroll back considerably after running a hook or interacting with a hook to find out what
precisely happened.

# The Example Hooks
## Carbon
Installing this hook causes the account to send a 1% carbon-offset txn each time another txn is sent from the account.
Install with:
```
cd carbon
node carbon
```
    
Test with (while still in the carbon account):
```
node pay
```

## Liteacc
Installing this hook causes the account to become a multi-party pool account with each user acquiring a unique dest
tag with which they can receive funds on the account.
Install with:
```
cd liteacc
node liteacc
```

Test with (for example):
```
node litepay1
node litepay2
node litepay2-sendout
```

## Firewall
Installing this hook causes transactions from blacklisted accounts to be rejected
Installing is a two step process, first create the blacklist storage account and install the blacklist itself:
```
cd firewall
node blacklist
```
    
Next install the firewall hook on the protected account
```
node firewall
```

Next attempt a payment to the protected account (this will succeed)
```
node firewall-pay1
```

Now blacklist the sending account
```    
node blacklist-add1
```

Finally attempt again to send from the blacklisted account to the protected account (this will now fail)
```    
node firewall-pay1
```

## Accept
This hook is a minimum example hook.
Installation:
```
cd accept
node accept
```

This hook simply passes every transaction that hits the account.

## Hook API
Preliminary documentation for the Hook API is found in hookapi.h. Further documentation will be published in future.
