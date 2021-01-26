/* blacklist admin key
{
  privateKey: 'ED55D3A139AF8F069FE93BB943FE3A46BAF70EA61E0DD02192D4A532D8E87627F0',
  publicKey: 'EDDC6D9E28CA0FE2D475FC021D226881666EA106FBD2222C8C2110368A49C9513C'
}
*/

const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const keypairs = require('ripple-keypairs')
const addr = require('ripple-address-codec')

const blacklist_pubkey = 'EDDC6D9E28CA0FE2D475FC021D226881666EA106FBD2222C8C2110368A49C9513C'
const blacklist_seckey = 'ED55D3A139AF8F069FE93BB943FE3A46BAF70EA61E0DD02192D4A532D8E87627F0'

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
        Flags: 1, // 1 = add, 0 = remove
        Template: [
            {
                Account: "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
            }
        ]
    });

    console.log("Blacklist-ADD TXN:")
    console.log(blacklist_instruction)

    let j = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: "100000",
        Destination: "rNsA4VzfZZydhGAvfHX3gdpcQMMoJafd6v",
        InvoiceID: blacklist_pubkey.slice(2),
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
        s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Added rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh [ root acc ] to blacklist on " +
                "rNsA4VzfZZydhGAvfHX3gdpcQMMoJafd6v")
            process.exit()
        }).catch ( e=> { console.log(e) } );
    });

}).then(() => {}).catch(console.error);

