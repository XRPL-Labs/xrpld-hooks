require('./utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  t.randomAccount();
    const account2 = t.randomAccount();
    t.fundFromGenesis(account).then(()=>
    {
        t.fundFromGenesis(account2).then(()=>
        {
            t.api.submit(
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
                ],
                Fee: "100000"
            }, {wallet: account}).then(x=>
            {
                t.assertTxnSuccess(x)
                console.log(x)
                t.api.submit(
                {
                    Account: account2.classicAddress,
                    TransactionType: "SetHook",
                    Fee: "100000",
                    Hooks: [
                        { Hook: {
                            HookHash: "348C7966C79737F6254C3D3866DBEE0AE1584E0751771F0588F898E65C7DFB84",
                            Flags: 0
                        }},
                        { Hook: {

                        }},
                        { Hook: {
                            HookHash: "348C7966C79737F6254C3D3866DBEE0AE1584E0751771F0588F898E65C7DFB84"
                        }}
                    ]
                }, {wallet: account2}).then(x=>
                {
                    t.assertTxnSuccess(x)
                    console.log(x);
                    t.api.submit(
                    {
                        Account: account2.classicAddress,
                        TransactionType: "SetHook",
                        Fee: "100000",
                        Flags: 0,
                        Hooks: [
                            { Hook: {
                                "Flags": t.hsfOVERRIDE,
                                "CreateCode": "",
                            }}
                        ]
                    }, {wallet: account2}).then(x=>
                    {
                        console.log(x);
                        t.assertTxnSuccess(x);

                        t.api.submit(
                        {
                            Account: account2.classicAddress,
                            TransactionType: "SetHook",
                            Fee: "100000",
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
                        }, {wallet: account2}).then(x=>
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



