const wasm = 'peggy.wasm'
if (process.argv.length < 3)
{
    console.log("Usage: node peggy <account family seed>")
    process.exit(1);
}


require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const secret  = process.argv[2];
    const account = t.xrpljs.Wallet.fromSeed(secret)
    t.feeSubmit(process.argv[2],
    {
        Account: account.classicAddress,
        TransactionType: "SetHook",
        Hooks: [
            {
                Hook: {
                    CreateCode: t.wasm(wasm),
                    HookApiVersion: 0,
                    HookNamespace: "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE",
                    HookOn: "0000000000000000",
                    Flags: t.hsfOVERRIDE
                }
            }
        ]
    }).then(x=>
    {
        t.assertTxnSuccess(x)
        console.log(x);
        process.exit(0);

    }).catch(t.err);
}).catch(e=>console.log(e));
