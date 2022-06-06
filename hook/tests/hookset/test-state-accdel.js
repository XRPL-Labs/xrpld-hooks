require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  t.randomAccount();
    const account2 =  t.randomAccount();
    t.fundFromGenesis([account, account2]).then(()=>
    {
        t.feeSubmit(account.seed,
        {
            Account: account.classicAddress,
            TransactionType: "SetHook",
            Hooks: [
                {
                    Hook: {
                        CreateCode: t.wasm('makestate.wasm'),
                        HookApiVersion: 0,
                        HookNamespace: "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF",
                        HookOn: "0000000000000000"
                    }
                },
                {   
                    Hook: {
                        CreateCode: t.wasm('makestate2.wasm'),
                        HookApiVersion: 0,
                        HookNamespace: "CAFEF00DCAFEF00DCAFEF00DCAFEF00DCAFEF00DCAFEF00DCAFEF00DCAFEF00D",
                        HookOn: "0000000000000000"
                    }
                },
                {   Hook: {} },
                {    Hook: {
                        CreateCode: t.wasm('checkstate.wasm'),
                        HookApiVersion: 0,
                        HookNamespace: "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF",
                        HookOn: "0000000000000000"
                    }
                }
            ]
        }).then(x=>
        {
            t.assertTxnSuccess(x)
            t.api.submit(
            {
                Account: account.classicAddress,
                TransactionType: "AccountSet",  // trigger hooks
                Fee: "100000"
            }, {wallet: account}).then(x=>
            {
                t.assertTxnSuccess(x)
                console.log(x);
                t.ledgerAccept(255).then(x=>
                {
                    // account delete
                    t.feeSubmit(account.seed,
                    {
                        Account: account.classicAddress,
                        TransactionType: "AccountDelete",
                        Destination: account2.classicAddress,
                        Flags: 2147483648
                    }).then(x=>
                    {
                        console.log(x);
                        t.assertTxnSuccess(x);
                        process.exit(0);
                    }).catch(t.err);
                }).catch(t.err);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



