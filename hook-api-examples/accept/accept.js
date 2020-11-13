const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const api = new RippleAPI({server: 'ws://localhost:6005'});
api.on('error', (errorCode, errorMessage) => {console.log(errorCode + ': ' + errorMessage);});
api.on('connected', () => {console.log('connected');});
api.on('disconnected', (code) => {console.log('disconnected, code:', code);});
api.connect().then(() => {
    // load the web assembly
    binary = fs.readFileSync('accept.wasm').toString('hex').toUpperCase();
    j = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "SetHook",
        CreateCode: binary,
        HookOn: '0000000000000000'
    }
    api.prepareTransaction(j).then( (x)=> 
    {
        let s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
            process.exit(0);        
        }).catch ( e=> { console.log(e) });
    });
}).then(() => {}).catch(console.error);
