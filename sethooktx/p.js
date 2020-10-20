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
  // code - [close code](https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent) sent by the server
  // will be 1000 if this was normal closure
  console.log('disconnected, code:', code);
});
api.connect().then(() => {
  /* insert code here */
    
    j = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: ""+ ( ~~(Date.now() / 1000) ),
        Destination: "rUCzEr6jrEyMpjhs4wSdQdz4g8Y382NxfM",
        LastLedgerSequence: 20,
        Fee: "100000"
    }
    api.prepareTransaction(j).then( (x)=> 
        {
        
        s= api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
        

        
        }).catch ( e=> { console.log(e) });


        } );
}).then(() => {
 // return api.disconnect();
}).catch(console.error);
