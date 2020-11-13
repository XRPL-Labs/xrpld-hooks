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

// end user firewall account
/*  Address
        rENDKpJtubdd9R7vJiY5oJ1sKXbZifXgUT
    Secret
        sn3ZaJikJ8mCF6XUHxnPYvbRBsGBK
*/
api.connect().then(() => {
    console.log("Sending 10000 XRP to rENDKpJtubdd9R7vJiY5oJ1sKXbZifXgUT from rUz4tCjPFUobvHoafM5MFGycg4qRuqLxw3")
    j = {
        Account: 'rUz4tCjPFUobvHoafM5MFGycg4qRuqLxw3',
        TransactionType: "Payment",
        Amount: "10000000000", // 10000 xrp
        Destination: "rENDKpJtubdd9R7vJiY5oJ1sKXbZifXgUT",
        LastLedgerSequence: 20,
        Fee: "100000"
    }
    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, 'shMWL6dozdMe7oqyPiNzhujXAU4qo')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
        }).catch ( e=> { console.log(e) } );
     });
}).then(() => {
}).catch(console.error);
