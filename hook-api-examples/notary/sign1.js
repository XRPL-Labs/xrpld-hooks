
const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const keypairs = require('ripple-keypairs')
const addr = require('ripple-address-codec')
const {multisignkeys} = require("./multisignkeys.js");

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

if (process.argv.length < 3)
{
    console.log("Must provide txn hash as cmdline argument");
    process.exit(1);
}
console.log(process.argv[2]);
api.connect().then(() => {
   



    let j = {
        Account: 'rJU4PWvRQBnoDdscWvpWrAqGogmtfk3RXM',
        TransactionType: "Payment",
        Amount: "1",
        Destination: "rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957",
        Fee: "100000",
        InvoiceID: process.argv[2]
    }

    api.prepareTransaction(j).then((x)=>
    {
        s = api.sign(x.txJSON, 'ss9bkDvdPjoTnJ6yFcECBgZwy7Qjt')
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
