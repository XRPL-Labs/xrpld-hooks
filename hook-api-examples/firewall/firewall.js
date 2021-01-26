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
    j = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: "1000000000", // 1000 xrp
        Destination: "rENDKpJtubdd9R7vJiY5oJ1sKXbZifXgUT",
        Fee: "100000"
    }
    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Sent 100 XRP to rENDKpJtubdd9R7vJiY5oJ1sKXbZifXgUT")
            
            binary = fs.readFileSync('firewall.wasm').toString('hex').toUpperCase();
            j = {
                Account: 'rENDKpJtubdd9R7vJiY5oJ1sKXbZifXgUT',
                TransactionType: "SetHook",
                CreateCode: binary,
                HookOn: '0000000000000000'
            }
            api.prepareTransaction(j).then( (x)=>
            {

                s= api.sign(x.txJSON, 'sn3ZaJikJ8mCF6XUHxnPYvbRBsGBK')
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage);
                    process.exit(0);


                }).catch ( e=> { console.log(e) });
            });
        }).catch ( e=> { console.log(e) } );
     });
}).then(() => {
}).catch(console.error);
