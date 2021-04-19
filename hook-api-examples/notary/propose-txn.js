if (process.argv.length < 6)
{
    console.log("Usage: node propose-txn \n" +
        "<first signer family seed> <multisig hook account> <xrp amount> " +
        "<destination account> [destination tag]")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const carrier_secret  = process.argv[2];
const carrier_address = keypairs.deriveAddress(keypairs.deriveKeypair(carrier_secret).publicKey)
const hook_account = process.argv[3];
const amount = BigInt(process.argv[4]) * 1000000n
const dest_acc = process.argv[5];
const dest_tag = (process.argv.length < 7 ? parseInt(process.argv[6]) : null);


const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const addr = require('ripple-address-codec')

const fs = require('fs');
const api = new RippleAPI({
    server: 'ws://localhost:6005'
});
api.on('error', (errorCode, errorMessage) => {
  console.log(errorCode + ': ' + errorMessage);
});
api.on('connected', () => {
  console.log('connected');
});
api.on('disconnected', (code) => {
    console.log('disconnected, code:', code);
});


function hexlify_memos(x)
{
    if (!("Memos" in x))
        return;

    for (y in x["Memos"])
    {
        for (a in x["Memos"][y])
        {
            let Fields = ["MemoFormat", "MemoType", "MemoData"];
            for (z in Fields)
            {
                if (Fields[z] in x["Memos"][y][a])
                {
                    let u = x["Memos"][y][a][Fields[z]].toUpperCase()
                    if (u.match(/^[0-9A-F]+$/))
                    {
                        x["Memos"][y][a][Fields[z]] = u;
                        continue;
                    }

                    x["Memos"][y][a][Fields[z]] =
                            ""+Buffer.from(x["Memos"][y][a][Fields[z]]).toString('hex').toUpperCase();
                }
            }
        }
    }
}
api.connect().then(() => {

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

    proposed_txn = bin.encode(proposed_txn);

    let j = {
        Account: carrier_address,
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

    }

    hexlify_memos(j)
    api.prepareTransaction(j).then((x)=>
    {
        s = api.sign(x.txJSON, carrier_secret)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            let countdown = (x)=>{
                if (x <= 0)
                    return console.log("")
                process.stdout.write(x + "... ");
                setTimeout(((x)=>{ return ()=>{countdown(x);} })(x-1), 1000);
            };
            countdown(6);
            setTimeout(
            ((txnhash)=>{
                return ()=>{
                    api.getTransaction(txnhash, {includeRawTransaction: true}).then(
                        x=>{
                            execs = JSON.parse(x.rawTransaction).meta.HookExecutions;
                            for (y in execs)
                            {
                                exec = execs[y].HookExecution;
                                if (exec.HookAccount == hook_account)
                                {
                                    console.log("Hook Returned: ",
                                        Buffer.from(exec.HookReturnString, 'hex').toString('utf-8'));
                                    process.exit(0);
                                }
                            }
                            console.log("Could not find return from hook");
                            process.exit(1);
                        });
                }
            })(s.id), 6000);
        }).catch ( e=> { console.log(e) } );
    });


}).then(() => {}).catch(console.error);
