// makes a generic payment to r3Worx88zJEckYcifVdqMvucLS7PNBP76N from root account
const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const api = new RippleAPI({
    server: 'wss://hooks-testnet.xrpl-labs.com/'
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
        Account: 'rhB2YdR84oe7Yp7uxfMgv62Dc91MMw478s',
        TransactionType: "Payment",
        Amount: ""+ ( ~~(Date.now() / 1000) ),
        Destination: "r3Worx88zJEckYcifVdqMvucLS7PNBP76N",
        Fee: "100000"
    }
    api.prepareTransaction(j).then( (x)=> 
    {
        s = api.sign(x.txJSON, 'shvMLWq6d4Jbf7wxp4jqNRVdbhPTg')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Done!")
            process.exit()  
        }).catch ( e=> { console.log(e) } );
     });
}).then(() => {
}).catch(console.error);
