if (process.argv.length < 6)
{
    console.log("Usage: node propose-txn \n" +
        "<first signer family seed> <multisig hook account> <xrp amount> " +
        "<destination account> [destination tag]")
    process.exit(1)
}

const hook_account = process.argv[3];
const amount = BigInt(process.argv[4]) * 1000000n
const dest_acc = process.argv[5];
const dest_tag = (process.argv.length < 7 ? parseInt(process.argv[6]) : null);

require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    const account = t.xrpljs.Wallet.fromSeed(process.argv[2]);
    let proposed_txn =
    {
        TransactionType: "Payment",
        Account:  hook_account,
        Destination: dest_acc,
        Amount: '' + amount,
        LastLedgerSequence: "4000000000", 
        Fee: "10000"
    }

    if (dest_tag !== null)
        proposed_txn["DestinationTag"] = dest_tag;

    proposed_txn = t.rbc.encode(proposed_txn);

    let host_txn = 
    {
        Account: account.classicAddress,
        TransactionType: "Payment",
        Amount: "1",
        Destination: hook_account,
        Fee: "10000",
        Memos: [
            {
                Memo:{
                    MemoData: proposed_txn,
                    MemoFormat: "unsigned/payload+1",
                    MemoType: "notary/proposed"
                }
            }
        ]
    };
    t.hex_memos(host_txn);
    t.feeSubmit(process.argv[2], host_txn).then(x=>
    {
        console.log(x);
        t.assertTxnSuccess(x);
        process.exit(0);
    }).catch(t.err);
}).catch(e=>console.log(e));
