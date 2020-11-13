const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const api = new RippleAPI({server: 'ws://localhost:6005'});
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
/*
 * Address
 *  rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957
 * Secret
 *  ssxdewtczG1DU63wWHfEJ4q5a6Dik
 */
    // first activate the lite account address
    var activate_carbon = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: "10000000000", // 1000 XRP
        Destination: "rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957",
        LastLedgerSequence: 20,
        Fee: "100000"
    }

    api.prepareTransaction(activate_carbon).then((x)=> 
    {
        s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
        
            // then upload the liteacc.wasm hook
            binary = fs.readFileSync('liteacc.wasm').toString('hex').toUpperCase();
            j = {
                Account: 'rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957',
                TransactionType: "SetHook",
                CreateCode: binary,
                HookOn: '0000000000000000'
            }
            api.prepareTransaction(j).then((x)=> 
            {
                s = api.sign(x.txJSON, 'ssxdewtczG1DU63wWHfEJ4q5a6Dik')
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage);
                    console.log("Done!")
                    process.exit() 
                }).catch ( e=> { console.log(e) });
            });
        }).catch (e=> { console.log(e) });
    });

}).then(() => {
 // return api.disconnect();
}).catch(console.error);
