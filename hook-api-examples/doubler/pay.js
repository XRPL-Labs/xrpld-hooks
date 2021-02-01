// makes a generic payment to r3Worx88zJEckYcifVdqMvucLS7PNBP76N from root account

/*
 *
 * Address
rGVvmYqMGwrNMz81wgcoxoCicU357zpAzK
Secret
shDoEchB6EkFxwP8LunsRoSJR6JSy
*/

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
        Account: 'rGVvmYqMGwrNMz81wgcoxoCicU357zpAzK',
        TransactionType: "Payment",
        Amount: "12345", // doubles to 24690
        Destination: "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
        Fee: "100000"
    }
    api.prepareTransaction(j).then( (x)=> 
    {
        s = api.sign(x.txJSON, 'shDoEchB6EkFxwP8LunsRoSJR6JSy')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Done!")
            process.exit()  
        }).catch ( e=> { console.log(e) } );
     });
}).then(() => {
}).catch(console.error);
