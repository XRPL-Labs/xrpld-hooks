require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  t.randomAccount();
    const account2 =  t.randomAccount();
    t.fundFromGenesis([account]).then(()=>
    {
        t.feeSubmit(account.seed,
        {
            Account: account.classicAddress,
            TransactionType: "SetHook",
            Hooks: [
                {
                    Hook: {
                        CreateCode: t.wasm('multiescrow.wasm'),
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
                Account: account.classicAddress,
                TransactionType: "Payment",
                Destination: "rfCarbonVNTuXckX6x2qTMFmFSnm6dEWGX",
                Amount: "10000000000"
            }).then(x=>
            {
                t.assertTxnSuccess(x)
                t.ledgerAccept(10).then(()=>{
                    t.fundFromGenesis([account2]).then(()=>
                    {
                        process.exit(0);
                    }).catch(t.err);
                }).catch(t.err);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



