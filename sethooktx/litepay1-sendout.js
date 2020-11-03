// LITE PAY ACCOUNT 1 
/*
r.deriveKeypair(r.generateSeed({algorithm: "ed25519"}))
{
  privateKey: 'ED9B24196C9A72C5BC575636DCFFAB3F4BC91AFBE335B8EDB0126942B8D3D08030',
  publicKey: 'EDC8822EE339D53EEC3F7C547C055614AAC26580B2A6462F4B72FDF3395F2392A5'
}
*/


const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const keypairs = require('ripple-keypairs')
const addr = require('ripple-address-codec')

const litepubkey = 'EDC8822EE339D53EEC3F7C547C055614AAC26580B2A6462F4B72FDF3395F2392A5';
const liteseckey = 'ED9B24196C9A72C5BC575636DCFFAB3F4BC91AFBE335B8EDB0126942B8D3D08030';

const liteaddr = keypairs.deriveAddress(litepubkey);

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
   


    let lite_payment = bin.encode(
    {
        Destination: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        DestinationTag: 0,
        SigningPubKey: litepubkey,
        Amount: process.argv.length > 2 ? process.argv[2] : "1000000",
        Sequence: Math.floor(Date.now()/1000)
    });

    console.log("Liteaccount TXN:")
    console.log(lite_payment)


    let j = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: "100000",
        Destination: "rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957",
        LastLedgerSequence: 20,
        InvoiceID: litepubkey.slice(2),
        Fee: "100000",
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
        s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Done!")
            process.exit()  
        }).catch ( e=> { console.log(e) } );
    });


}).then(() => {
 // return api.disconnect();
}).catch(console.error);



    /*
    var encoded = bin.encodeForSigning(j)

    var sk = keypairs.deriveKeypair('snoPBrXtMeMyMHUVTgbuqAfg1SUTb')

    var signature = keypairs.sign(encoded, sk.privateKey)

    j['SigningPubKey'] = sk.publicKey;
    j['TxnSignature'] = signature;

    var encodedSigned = bin.encode(j)
    console.log(encodedSigned)
*/
