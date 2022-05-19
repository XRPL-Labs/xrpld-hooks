require('./utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  t.randomAccount();
    t.fundFromGenesis(account).then(()=>
    {
        t.feeTxn(account.seed, 
        {
            Account: account.classicAddress,
            TransactionType: "SetHook",
            Hooks: [
                {
                    Hook: {
                        CreateCode: t.wasm('accept.wasm'),
                        HookApiVersion: 0,
                        HookNamespace: "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF",
                        HookOn: "0000000000000000"
                    }
                }
            ]
        }).then(txn => {
            console.log(txn)
            t.api.submit(txn, {wallet: account}).then(s=>
            {
                t.assertTxnSuccess(s);
                console.log(s);
                process.exit(0);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
});

