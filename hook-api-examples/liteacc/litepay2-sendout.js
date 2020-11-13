// LITE PAY ACCOUNT 2                                                                                                  
/*                                                                                                                     
> r.deriveKeypair(r.generateSeed({algorithm: "ed25519"}))                                                              
{                                                                                                                      
  privateKey: 'EDC8647DC1FE3F64D8B9FFDF50E0D56063A124D631B413D3DDBF64944B137D3E3E',                                    
  publicKey: 'EDAB537E44B1B0188154C9B7B7D1B1CD351109406C789F4B65F6719E332CEDC764'                                      
}                                                                                                                      
*/


const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const keypairs = require('ripple-keypairs')
const addr = require('ripple-address-codec')

const litepubkey = 'EDAB537E44B1B0188154C9B7B7D1B1CD351109406C789F4B65F6719E332CEDC764';
const liteseckey = 'EDC8647DC1FE3F64D8B9FFDF50E0D56063A124D631B413D3DDBF64944B137D3E3E';

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
        Destination: 'rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957', 
        DestinationTag: 1,
        SourceTag: 2,
        PublicKey: litepubkey,
        Amount: "35000000", // 35 XRP
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
