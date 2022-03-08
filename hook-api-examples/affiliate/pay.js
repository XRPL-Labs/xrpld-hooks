if (process.argv.length < 6)
{
    console.log("Usage: node pay <source family seed> <amount xrp> <destination account> <refund address memo>")
    process.exit(1)
}


const wssUrl = "wss://hooks-testnet.xrpl-labs.com"

const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];

const pair = keypairs.deriveKeypair(secret)


const address = keypairs.deriveAddress(pair.publicKey)
const amount = BigInt(process.argv[3]) * 1000000n
const dest = process.argv[4];
const refund_address = process.argv[5];


const bin = require('ripple-binary-codec')



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





const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const api = new RippleAPI({server: wssUrl});
api.on('error', (errorCode, errorMessage) => {console.log(errorCode + ': ' + errorMessage);});
api.on('connected', () => {console.log('connected');});
api.on('disconnected', (code) => {console.log('disconnected, code:', code);});
api.connect().then(() => {

    let affiliate = bin.encode(
    {
        Sequence: Math.floor(Date.now()/1000),
        Flags: 1, //operation, // 1 = add, 0 = remove
        Template: [
            {
                Account: refund_address
            }
        ]
    });


    let j = {
        Account: address,
        Amount: "" + amount,
        Destination: dest,
        TransactionType: "Payment",
        Fee: "100000",
        Memos: [
            {
                Memo:{
                    MemoData: affiliate,
                    MemoFormat: "signed/payload+1",
                    MemoType: "liteacc/payment"
                }
            },
            {
                Memo:{
                    MemoData: keypairs.sign(affiliate, pair.privateKey),
                    MemoFormat: "signed/signature+1",
                    MemoType: "liteacc/signature"
                }
            },
            {
                Memo:{
                    MemoData: pair.publicKey,
                    MemoFormat: "signed/publickey+1",
                    MemoType: "liteacc/publickey"
                }
            }
        ]

    }


    hexlify_memos(j)

    api.prepareTransaction(j).then((x)=>
    {
        s = api.sign(x.txJSON, secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
            process.exit(0);
        }).catch (e=> { console.log(e) });
    });
}).then(() => {}).catch(console.error);
