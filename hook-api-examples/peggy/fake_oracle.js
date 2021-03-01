/*
 * Address
rH23joJXj7giNts39XRaVA2e7UFLvziveW
Secret
ss638spzrYNK34pxxGQLeW884JNPi

Address
rnBQxAy2jL79rhS8ybEcYMBSQyW1FEThzH
Secret
ssqXNinE7tHt31WrzhsHKY9ZYw7pp
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
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: ""+ ( ~~(Date.now() / 1000) ),
        Destination: "rH23joJXj7giNts39XRaVA2e7UFLvziveW",
        Fee: "100000"
    }
    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            j = {
                Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
                TransactionType: "Payment",
                Amount: ""+ ( ~~(Date.now() / 1000) ),
                Destination: "rnBQxAy2jL79rhS8ybEcYMBSQyW1FEThzH",
                Fee: "100000"
            }
            api.prepareTransaction(j).then( (x)=>
            {
                s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage)
                    j = {
                        Account: 'rnBQxAy2jL79rhS8ybEcYMBSQyW1FEThzH',
                        TransactionType: "TrustSet",
                        Fee: "100000",
                        Flags: "262144",
                        LimitAmount: {
                          currency: "USD",
                          issuer: "rH23joJXj7giNts39XRaVA2e7UFLvziveW",
                          value: "0.42"
                        }
                    }
                    api.prepareTransaction(j).then( (x)=>
                    {
                        s = api.sign(x.txJSON, 'ssqXNinE7tHt31WrzhsHKY9ZYw7pp')
                        console.log(s)
                        api.submit(s.signedTransaction).then( response => {
                            console.log(response.resultCode, response.resultMessage)
                            console.log("Done!")
                            process.exit()
                        }).catch ( e=> { console.log(e) } );
                    });
                }).catch ( e=> { console.log(e) } );
            });
        }).catch ( e=> { console.log(e) } );
     });
}).then(() => {
}).catch(console.error);
