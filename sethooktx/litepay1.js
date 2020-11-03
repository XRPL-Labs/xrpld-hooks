// LITE PAY ACCOUNT 1 
/*
r.deriveKeypair(r.generateSeed({algorithm: "ed25519"}))
{
  privateKey: 'ED9B24196C9A72C5BC575636DCFFAB3F4BC91AFBE335B8EDB0126942B8D3D08030',
  publicKey: 'EDC8822EE339D53EEC3F7C547C055614AAC26580B2A6462F4B72FDF3395F2392A5'
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
        InvoiceID: "C8822EE339D53EEC3F7C547C055614AAC26580B2A6462F4B72FDF3395F2392A5",
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
