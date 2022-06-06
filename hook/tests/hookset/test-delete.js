require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  (process.argv.length < 3 ? t.randomAccount() :
        t.xrpljs.Wallet.fromSeed(process.argv[2]));
    
    t.fundFromGenesis(account).then(()=>
    {
        t.feeSubmit(account.seed,
        {
            Account: account.classicAddress,
            TransactionType: "SetHook",
            Hooks: [{Hook:{}}],
            Fee: "100000"
        }, {wallet: account}).then(x=>
        {
            t.assertTxnSuccess(x)
            console.log(x)
            t.feeSubmit(account.seed,
            {
                Account: account.classicAddress,
                TransactionType: "SetHook",
                Hooks: [
                    {
                        Hook: {
                            Flags: t.hsfOVERRIDE,
                            CreateCode: t.wasm('accept.wasm'),
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
                    TransactionType: "SetHook",
                    Hooks: [
                        {
                            Hook: {
                                Flags: t.hsfOVERRIDE,
                                CreateCode: ""          // hook delete
                            }
                        }
                    ],
                }).then(x=>
                {
                    t.assertTxnSuccess(x)
                    console.log(x);
                    process.exit(0);
                }).catch(t.err);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



