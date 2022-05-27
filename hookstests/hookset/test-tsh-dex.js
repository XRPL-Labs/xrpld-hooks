require('./utils-tests.js').TestRig('ws://localhost:6005').then(t=>{
    const holder1 =  t.randomAccount();
    const holder2 =  t.randomAccount();
    const holder3 =  t.randomAccount();
    const holder4 =  t.randomAccount();
    const issuer = t.randomAccount();


    t.fundFromGenesis([issuer, holder1, holder2, holder3, holder4]).then(()=>
    {
        t.setTshCollect([holder1, holder2, holder3, holder4]).then(()=>
        {
            t.trustSet(issuer, "IOU", 10000, [holder1,holder2,holder3,holder4]).then(()=>
            {
                t.feeSubmitAcceptMultiple(
                {
                    TransactionType: "SetHook",
                    Hooks: [
                        {
                            Hook:
                            {
                                CreateCode: t.wasm('aaw.wasm'),
                                HookApiVersion: 0,
                                HookNamespace: "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF",
                                HookOn: "0000000000000000",
                                Flags: t.hsfCOLLECT
                            }
                        }
                    ]
                }, [holder1,holder2,holder3,holder4]).then(()=>
                {

                    req = {};

                    req[holder1.classicAddress] = 500;
                    req[holder2.classicAddress] = 200;
                    req[holder3.classicAddress] = 80;

                    t.issueTokens(issuer, "IOU", req).then(()=>
                    {

                        const coffer = (offerer, issuer, currency, iouamount, dropsamount, buying) =>
                        {
                            if (typeof(issuer.classicAddress) != 'undefined')
                                issuer = issuer.classicAddress;

                            return new Promise((resolve, reject) =>
                            {
                                let txn = 
                                {
                                    Account: offerer.classicAddress,
                                    TransactionType: "OfferCreate",
                                };

                                let key1 = (buying ? "TakerGets" : "TakerPays");
                                let key2 = (buying ? "TakerPays" : "TakerGets");

                                txn[key1] = dropsamount + "";
                                txn[key2] = {
                                    "currency": currency,
                                    "issuer": issuer,
                                    "value": iouamount + ""
                                };

                                t.feeSubmitAccept(offerer.seed, txn).then(x=>
                                {
                                    console.log(x);
                                    t.assertTxnSuccess(x);
                                    resolve(x);
                                }).catch(e=>reject(e));
                            });
                        };

                        coffer(holder1, issuer, "IOU", 10, 250000, false).then(()=>         //  q= 25000
                        {
                            coffer(holder2, issuer, "IOU", 12, 100000, false).then(()=>     //  q= 8333
                            {
                                coffer(holder3, issuer, "IOU", 14, 80000, false).then(()=>  //  q= 5714 
                                {
                                    coffer(holder4, issuer, "IOU", 30, 350000, true).then((x)=>
                                    {
                                        t.ledgerAccept().then(()=>
                                        {
                                            t.fetchMetaHookExecutions(x).then(h=>
                                            {
                                                t.fetchMeta(x).then(m=>
                                                {
                                                    t.log(x);
                                                    delete m.HookExecutions;
                                                    t.log(m);
                                                    t.log(h);
                                                    console.log("Issuer:", issuer.classicAddress);
                                                    console.log("Holder 1: ", holder1.classicAddress);
                                                    console.log("Holder 2: ", holder2.classicAddress);
                                                    console.log("Holder 3: ", holder3.classicAddress);
                                                    console.log("Buyer: ", holder4.classicAddress);
                                                    console.log("(cd b; ./rippled book_offers XRP 'IOU/" + issuer.classicAddress + "');");
                                                    console.log("(cd b; ./rippled account_lines " + issuer.classicAddress + ");");
                                                    process.exit(0);
                                                }).catch(t.err);
                                            }).catch(t.err);
                                        }).catch(t.err);
                                    }).catch(t.err);
                                }).catch(t.err);
                            }).catch(t.err);
                        }).catch(t.err);
                    }).catch(t.err);
                }).catch(t.err);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



