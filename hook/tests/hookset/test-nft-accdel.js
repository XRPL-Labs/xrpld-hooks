require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account1 =  t.randomAccount();
    const account2 =  t.randomAccount();
    t.fundFromGenesis([account1, account2]).then(()=>
    {
        t.ledgerAccept(256).then(x=>
        {
            t.feeSubmitAccept(account1.seed,
            {
              NFTokenTaxon: 0,
              TransactionType: "NFTokenMint",
              Account: account1.classicAddress,
              TransferFee: 314,
              Flags: 8,
              URI: "697066733A2F2F62616679626569676479727A74357366703775646D37687537367568377932366E6634646675796C71616266336F636C67747179353566627A6469",
              Memos: [
                    {
                        "Memo": {
                            "MemoType":
                              "687474703A2F2F6578616D706C652E636F6D2F6D656D6F2F67656E65726963",
                            "MemoData": "72656E74"
                        }
                    }
                ]
            }).then(x=>
            {
                console.log(x)
                t.assertTxnSuccess(x)

                const id = t.nftid(account1.classicAddress, 8, 314, 0, 0);

                t.feeSubmit(account1.seed,
                {
                    TransactionType: "NFTokenCreateOffer",
                    Account: account1.classicAddress,
                    NFTokenID: id,
                    Amount: "1",
                    Flags: 1
                }).then(x=>
                {

                    console.log(x);
                    t.assertTxnSuccess(x)
                    t.fetchMeta(x.result.tx_json.hash).then(m=>
                    {
                        // RH UPTO
                        t.log(m);

                        process.exit(0);
                    }).catch(t.err);
/*
                    t.feeSubmit(account1.seed,
                    {
                        TransactionType: "AccountDelete",
                        Account: account1.classicAddress,
                        Destination: account2.classicAddress,
                        Flags: 2147483648
                    }).then(x=>
                    {
                        t.assertTxnSuccess(x)
                        
                        process.exit(0);
                    });
                    */

                }).catch(t.err);
            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
})



