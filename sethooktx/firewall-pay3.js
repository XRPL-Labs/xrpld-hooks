const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const keypairs = require('ripple-keypairs')
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


api.connect().then(() => {


    j = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: "20000000000", // 20k xrp
        Destination: "rnZWXnfmQoRq9mHNB1f9xuWFDj32peHT2Z",
        LastLedgerSequence: 20,
        Fee: "100000"
    }
    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Activated/sent 20k xrp to rnZWXnfmQoRq9mHNB1f9xuWFDj32peHT2Z")

            j = {
                Account: 'rnZWXnfmQoRq9mHNB1f9xuWFDj32peHT2Z',
                TransactionType: "Payment",
                Amount: "10000000000", // 10k xrp
                Destination: "rENDKpJtubdd9R7vJiY5oJ1sKXbZifXgUT",
                LastLedgerSequence: 20,
                Fee: "100000"
            }
            api.prepareTransaction(j).then( (x)=>
            {
                s = api.sign(x.txJSON, 'shb9YysJRKogBbD1jScDNQzGw1iVr')
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage)
                    console.log("Sent 10k xrp from rnZWXnfmQoRq9mHNB1f9xuWFDj32peHT2Z to " +
                        "rENDKpJtubdd9R7vJiY5oJ1sKXbZifXgUT");
                }).catch ( e=> { console.log(e) } );
             });
        }).catch ( e=> { console.log(e) } );
     });

}).then(() => {
}).catch(console.error);

