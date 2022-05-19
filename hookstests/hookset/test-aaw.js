require('./utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account1 =  t.randomAccount();
    const account2 =  t.randomAccount();
    t.fundFromGenesis(account1).then(()=>
    {
        t.fundFromGenesis(account2).then(()=>
        {
            t.feeSubmit(account1.seed,
            {
                Account: account1.classicAddress,
                TransactionType: "SetHook",
                Hooks: [
                    {
                        Hook: {
                            CreateCode: t.wasm('aaw.wasm'),
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
                t.api.submit(
                {
                    Account: account2.classicAddress,
                    TransactionType: "AccountSet",
                    Fee: "100000"
                }, {wallet: account2}).then(x=>
                {
                    t.assertTxnSuccess(x)
                    process.exit(0);
                }).catch(t.err);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



