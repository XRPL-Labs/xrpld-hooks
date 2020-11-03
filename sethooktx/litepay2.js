// LITE PAY ACCOUNT 1 
/*
> r.deriveKeypair(r.generateSeed({algorithm: "ed25519"}))
{
  privateKey: 'EDC8647DC1FE3F64D8B9FFDF50E0D56063A124D631B413D3DDBF64944B137D3E3E',
  publicKey: 'EDAB537E44B1B0188154C9B7B7D1B1CD351109406C789F4B65F6719E332CEDC764'
}
*/
const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const keypairs = require('ripple-keypairs')
const addr = require('ripple-address-codec')
const process = require('process')

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
   
    j = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: ( process.argv.length == 3 ? process.argv[2] : ""+ ( ~~(Date.now() / 1000) )) ,
        Destination: "rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957",
        LastLedgerSequence: 20,
        InvoiceID: "AB537E44B1B0188154C9B7B7D1B1CD351109406C789F4B65F6719E332CEDC764",
        Fee: "100000",
           
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
