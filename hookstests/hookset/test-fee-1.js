require('./utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account =  t.randomAccount();
    t.fundFromGenesis(account).then(()=>
    {
        t.api.submit(
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
            ],
            Fee: t.wasmFee('accept.wasm')
        }, {wallet: account}).then(x=>
        {
            t.assertTxnSuccess(x)
            console.log(x);
            t.pay_mock(t.genesisseed, 100, account).then(ser=>
            {
                t.fee(ser).then(fees =>
                {
                    let base_drops = fees.base_fee 
                    console.log("base_drops", base_drops)
                    
                    let txn = t.rbc.decode(ser);
                    delete txn['SigningPubKey']
                    delete txn['Fee']
                    txn['Fee'] = base_drops + '';

                    console.log(txn)
                    t.api.submit(txn, {wallet: account}).then(s=>
                    {
                        t.assertTxnSuccess(s);
                        console.log(s);
                        txn['Fee'] = (base_drops - 20) + '';
                        txn['Sequence'] = (txn['Sequence'] + 1) ;
                        console.log(txn)
                        t.api.submit(txn, {wallet: t.genesis}).then(s=>
                        {
                            t.assertTxnFailure(s);
                            console.log(s)
                            process.exit(0);
                        });
                    }).catch(t.err);
                });
            });
        }).catch(t.err);
    }).catch(t.err);
})



