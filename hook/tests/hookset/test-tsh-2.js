require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
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
                            CreateCode: t.wasm('rollback.wasm'),
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
                t.feeSubmit(account2.seed,
                {
                    Account: account2.classicAddress,
                    TransactionType: "SignerListSet",
                    SignerQuorum: 1,
                    SignerEntries:
                    [
                        {
                            SignerEntry:
                            {
                                Account: account1.classicAddress,
                                SignerWeight: 1
                            }
                        }
                    ]
                }).then(x=>
                {
                    t.assertTxnFailure(x)
                    process.exit(0);
                }).catch(t.err);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



