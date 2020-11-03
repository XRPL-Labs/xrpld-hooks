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

const litepubkey = 'EDC8647DC1FE3F64D8B9FFDF50E0D56063A124D631B413D3DDBF64944B137D3E3E';
const liteseckey = 'EDAB537E44B1B0188154C9B7B7D1B1CD351109406C789F4B65F6719E332CEDC764';

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
api.connect().then(() => {
   

    i = {
        Destination: 'rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957',
        DestinationTag: 1,
        SigningPubKey: litepubkey,
        Amount: process.argv.length > 2 ? process.argv[2] : "1000000"
    }

    i['TxnSignature'] = keypairs.sign(bin.encodeForSigning(i), liteseckey);

    let memo_format = "xrpl/signed-tx"
    let memo_type = "txblob"
    let memo_data = bin.encode(i)

    console.log("Liteaccount TXN:")
    console.log(memo_data)

    memo_format = (Buffer.from(memo_format).toString('hex').toUpperCase());
    memo_type = (Buffer.from(memo_type).toString('hex').toUpperCase());

    j = {
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
                    MemoData: memo_data,
                    MemoType: memo_type,
                    MemoFormat: memo_format
                }
            }
        ]
           
    }



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
