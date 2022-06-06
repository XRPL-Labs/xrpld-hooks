require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account1 =  t.randomAccount();
    t.fundFromGenesis(account1).then(()=>
    {
        t.feeSubmit(account1.seed,
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

            console.log(x);
            t.assertTxnSuccess(x)
            process.exit(0);
        }).catch(t.err);
    }).catch(t.err);
})



