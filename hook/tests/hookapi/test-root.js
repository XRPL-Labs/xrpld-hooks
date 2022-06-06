const wasmFn = 'root.wasm';

require('../hookset/utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  t.randomAccount();
    t.fundFromGenesis(account).then(()=>
    {
        t.feeSubmit(account.seed,
        {
            Account: account.classicAddress,
            TransactionType: "SetHook",
            Hooks: [
                {
                    Hook: {
                        CreateCode: t.wasm(wasmFn),
                        HookApiVersion: 0,
                        HookNamespace: "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF",
                        HookOn: "0000000000000000"
                    }
                }
            ]
        }).then(x=>
        {
            t.assertTxnSuccess(x)
            console.log(x);
            t.feeSubmit(account.seed,
            {
                TransactionType: "AccountSet",
                Account: account.classicAddress
            }).then(x=>
            {
                t.assertTxnSuccess(x)
                process.exit(0);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



