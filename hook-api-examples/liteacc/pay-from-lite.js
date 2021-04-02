if (process.argv.length < 8)
{
    console.log("Usage: node pay-from-lite \n" +
        "<msg carrier family seed> <lite hook account> <lite acc secret> <lite acc tag> <xrp amount> " + 
        "<destination account> [destination tag]")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const carrier_secret  = process.argv[2];
const carrier_address = keypairs.deriveAddress(keypairs.deriveKeypair(carrier_secret).publicKey)
const hook_account = process.argv[3];
const liteacc_secret = process.argv[4];
const litekeys = keypairs.deriveKeypair(liteacc_secret);
const litetag = parseInt(process.argv[5])

const litepubkey = litekeys.publicKey;
const liteseckey = litekeys.privateKey; 

const amount = BigInt(process.argv[6]) * 1000000n
const dest_acc = process.argv[7];
const dest_tag = ( process.argv.length < 9 ? process.argv[8] : null );

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

    // RH TODO: fetch the tag from the hook state and set it as a source tag
    let lite_payment =
    {
        Destination: dest_acc,
        PublicKey: litepubkey,
        SourceTag: litetag,
        Amount: '' + amount,
        Sequence: Math.floor(Date.now()/1000)
    }

    if (dest_tag !== null)
        lite_payment['DestinationTag'] = dest_tag;

    lite_payment = bin.encode(lite_payment);


    let j = {
        Account: carrier_address,
        TransactionType: "Payment",
        Amount: '' + amount,
        Destination: hook_account,
        InvoiceID: litepubkey.slice(2),
        Fee: "10000",
        Memos: [
            {
                Memo:{
                    MemoData: lite_payment,
                    MemoFormat: "signed/payload+1",
                    MemoType: "liteacc/payment"
                }
            },
            {
                Memo:{
                    MemoData: keypairs.sign(lite_payment, liteseckey),
                    MemoFormat: "signed/signature+1",
                    MemoType: "liteacc/signature"
                }
            },
            {
                Memo:{
                    MemoData: litepubkey,
                    MemoFormat: "signed/publickey+1",
                    MemoType: "liteacc/publickey"
                }
            }
        ]

    }

    hexlify_memos(j)
    console.log(JSON.stringify(j))

    api.prepareTransaction(j).then((x)=>
    {
        s = api.sign(x.txJSON, carrier_secret)
        console.log(s)
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

