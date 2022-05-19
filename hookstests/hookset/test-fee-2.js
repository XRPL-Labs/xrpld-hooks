require('./utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  t.randomAccount();
    t.fundFromGenesis(account).then(()=>
    {
        let txn_to_send = 
        {
            SigningPubKey: '',
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
            ],
            SigningPubKey: ''
        };

        let wal = t.xrpljs.Wallet.fromSeed(account.seed);
        t.api.prepareTransaction(txn_to_send, {wallet: wal}).then(txn => 
        {
            let ser = t.rbc.encode(txn);
            t.fee(ser).then(fees =>
            {
                console.log(fees)
                let base_drops = fees.base_fee 
                console.log("base_drops", base_drops)
                
                delete txn_to_send['SigningPubKey']
                txn_to_send['Fee'] = base_drops + '';

                t.api.prepareTransaction(txn_to_send, {wallet: wal}).then(txn => 
                {
                    console.log(txn)
                    t.api.submit(txn, {wallet: wal}).then(s=>
                    {
                        t.assertTxnSuccess(s);
                        console.log(s);
                        process.exit(0);
                    }).catch(t.err);
                }).catch(t.err);
            });


        }).catch(t.err);
    }).catch(t.err);
});

