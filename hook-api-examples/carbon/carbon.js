// Install the Carbon hook on rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh
// After which all payments from rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh will trigger a 1% payment to the
// rfCarbonVNTuXckX6x2qTMFmFSnm6dEWGX address
const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const api = new RippleAPI({server: 'wss://hooks-testnet.xrpl-labs.com/'});
api.on('error', (errorCode, errorMessage) => {console.log(errorCode + ': ' + errorMessage);});
api.on('connected', () => {console.log('connected');});
api.on('disconnected', (code) => {console.log('disconnected, code:', code);});
api.connect().then(() => {

    // first activate the rfCarbon address
    var activate_carbon = {
        Account: 'rhB2YdR84oe7Yp7uxfMgv62Dc91MMw478s',
        TransactionType: "Payment",
        Amount: ""+ ( ~~(Date.now() / 1000) ),
        Destination: "rfCarbonVNTuXckX6x2qTMFmFSnm6dEWGX",
        Fee: "100000"
    }

    api.prepareTransaction(activate_carbon).then((x)=> 
    {
        s = api.sign(x.txJSON, 'shvMLWq6d4Jbf7wxp4jqNRVdbhPTg')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
        
            // then upload the carbon.wasm hook
            binary = fs.readFileSync('carbon.wasm').toString('hex').toUpperCase();
            j = {
                Account: 'rhB2YdR84oe7Yp7uxfMgv62Dc91MMw478sh',
                TransactionType: "SetHook",
                CreateCode: binary,
                HookOn: '0000000000000000'
            }
            api.prepareTransaction(j).then((x)=> 
            {
                s = api.sign(x.txJSON, 'shvMLWq6d4Jbf7wxp4jqNRVdbhPTg')
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage);
                    console.log("Carbon Hook installed")
                    process.exit() 
                }).catch ( e=> { console.log(e) });
            });
        }).catch (e=> { console.log(e) });
    });

}).then(() => {
 // return api.disconnect();
}).catch(console.error);
