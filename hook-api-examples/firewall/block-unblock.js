/* blacklist admin key ((( change this ))) - r.deriveKeypair('sEd7CGWXiFPazNncZJsxD11h1sHJCtm') */
const blacklist_pubkey = 'EDDC6D9E28CA0FE2D475FC021D226881666EA106FBD2222C8C2110368A49C9513C'
const blacklist_seckey = 'ED55D3A139AF8F069FE93BB943FE3A46BAF70EA61E0DD02192D4A532D8E87627F0'

if (process.argv.length < 5)
{
    console.log("Usage: node block-unblock <blacklist family seed> <account to block or unblock> ( block | unblock ) ")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)
const account_to_block = process.argv[3];
const operation = (process.argv[4] == 'block' ? 1 : 0)


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

// turn memo fields into uppercase hex for ripple-lib
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

api.connect().then(() =>
{

    // the blacklist instruction is encoded as an stobject and stored in the memo field of the tx
    let blacklist_instruction = bin.encode(
    {
        Sequence: Math.floor(Date.now()/1000),
        Flags: operation, // 1 = add, 0 = remove
        Template: [
            {
                Account: account_to_block
            }
        ]
    });

    let j = {
        Account: address,
        TransactionType: "AccountSet",
        Fee: "100000",
        Memos: [
            {
                Memo:{
                    MemoData: blacklist_instruction,
                    MemoFormat: "signed/payload+1",
                    MemoType: "liteacc/payment"
                }
            },
            {
                Memo:{
                    MemoData: keypairs.sign(blacklist_instruction, blacklist_seckey),
                    MemoFormat: "signed/signature+1",
                    MemoType: "liteacc/signature"
                }
            },
            {
                Memo:{
                    MemoData: blacklist_pubkey,
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
        s = api.sign(x.txJSON, secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            process.exit(0)
        }).catch ( e=> { console.log(e) } );
    });

}).then(() => {}).catch(console.error);

