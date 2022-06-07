if (process.argv.length < 5)
{
    console.log("Usage: node sign <signer family seed> <notary hook account> <proposal id>")
    process.exit(1)
}

const secret  = process.argv[2];
const hook_account = process.argv[3];
const proposal = process.argv[4];

require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account = t.xrpljs.Wallet.fromSeed(process.argv[2]);
    t.fundFromGenesis([account.classicAddress]).then(()=>
    {
        t.feeSubmit(process.argv[2], 
        {
            Account: account.classicAddress,
            TransactionType: "Payment",
            Amount: "1",
            Destination: hook_account,
            InvoiceID: proposal
        }).then(x=>
        {
            console.log(x);
            t.assertTxnSuccess(x);
            process.exit(0);
        }).catch(t.err);
    }).catch(t.err);
}).catch(e=>console.log(e));
