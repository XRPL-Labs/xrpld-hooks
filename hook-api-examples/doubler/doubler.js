//Author NeilH
//(joke) test hook that doubles incoming XRP payments and sends it back
const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const api = new RippleAPI({server: 'ws://localhost:6005'});
api.on('error', (errorCode, errorMessage) => {console.log(errorCode + ': ' + errorMessage);});
api.on('connected', () => {console.log('connected');});
api.on('disconnected', (code) => {console.log('disconnected, code:', code);});
api.connect().then(() => {

    // first activate the rfCarbon address
    var activate_carbon = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: ""+ ( ~~(Date.now() / 1000) ),
        Destination: "rGVvmYqMGwrNMz81wgcoxoCicU357zpAzK",
        Fee: "100000"
    }

    api.prepareTransaction(activate_carbon).then((x)=> 
    {
        s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
        
            // then upload the doubler.wasm hook
            binary = fs.readFileSync('doubler.wasm').toString('hex').toUpperCase();
            j = {
                Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
                TransactionType: "SetHook",
                CreateCode: binary,
                HookOn: '0000000000000000'
            }
            api.prepareTransaction(j).then((x)=> 
            {
                s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage);
                    console.log("Carbon Hook installed on rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh [ root acc ]")
                    process.exit() 
                }).catch ( e=> { console.log(e) });
            });
        }).catch (e=> { console.log(e) });
    });

}).then(() => {
 // return api.disconnect();
}).catch(console.error);
