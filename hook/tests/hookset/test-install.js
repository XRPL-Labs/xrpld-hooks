require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  t.randomAccount();
    const account2 = t.randomAccount();
    t.fundFromGenesis(account).then(()=>
    {
        t.fundFromGenesis(account2).then(()=>
        {

            let hash = t.hookHash('checkstate.wasm')
            t.feeSubmit(account.seed,
            {
                Account: account.classicAddress,
                TransactionType: "SetHook",
                Hooks: [
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
                console.log(x)
                t.feeSubmit(account2.seed,
                {
                    Account: account2.classicAddress,
                    TransactionType: "SetHook",
                    Fee: "100000",
                    Hooks: [
                        { Hook: {
                            HookHash: hash
                        }},
                        { Hook: {

                        }},
                        { Hook: {
                            HookHash: hash
                        }}
                    ]
                }).then(x=>
                {
                    t.assertTxnSuccess(x)
                    console.log(x);
                    t.feeSubmit(account2.seed,
                    {
                        Account: account2.classicAddress,
                        TransactionType: "SetHook",
                        Flags: 0,
                        Hooks: [
                            { Hook: {
                                "Flags": t.hsfOVERRIDE,
                                "CreateCode": "",
                            }}
                        ]
                    }).then(x=>
                    {
                        console.log(x);
                        t.assertTxnSuccess(x);

                        t.feeSubmit(account2.seed,
                        {
                            Account: account2.classicAddress,
                            TransactionType: "SetHook",
                            Flags: 0,
                            Hooks: [
                                {Hook:{}},
                                {Hook:{}},
                                { Hook: {
                                    HookParameters:
                                    [
                                        {HookParameter: {
                                            HookParameterName: "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                                            HookParameterValue: "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB"
                                        }}
                                    ]
                                }}
                            ]
                        }).then(x=>
                        {
                            console.log(x);
                            t.assertTxnSuccess(x);

                            console.log("account 1 has the creation: ", account.classicAddress);
                            console.log("account 2 has the install and delete and update: ", account2.classicAddress);
                            process.exit(0);
                        }).catch(t.err);
                    }).catch(t.err);
                }).catch(t.err);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



