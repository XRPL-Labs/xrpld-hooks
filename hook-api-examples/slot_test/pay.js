// makes a generic payment to r3Worx88zJEckYcifVdqMvucLS7PNBP76N from root account
const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
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
        Amount: ""+ ( ~~(Date.now() / 1000) ),
        Destination: "r3Worx88zJEckYcifVdqMvucLS7PNBP76N",
        LastLedgerSequence: 20,
        Fee: "100000"
    }
    api.prepareTransaction(j).then( (x)=> 
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
}).catch(console.error);
