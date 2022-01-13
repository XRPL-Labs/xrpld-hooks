const wssUrl = "wss://hooks-testnet.xrpl-labs.com"

const RippleAPI = require('ripple-lib').RippleAPI;
const keypairs = require('ripple-keypairs');
const addr = require('ripple-address-codec')

const fs = require('fs');
const api = new RippleAPI({server: wssUrl});
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)

api.on('error', (errorCode, errorMessage) => {console.log(errorCode + ': ' + errorMessage);});
api.on('connected', () => {console.log('connected');});
api.on('disconnected', (code) => {console.log('disconnected, code:', code);});
api.connect().then(() => {
    binary = fs.readFileSync('affiliate.wasm').toString('hex').toUpperCase();
    j = {
        Account: address,
        TransactionType: "SetHook",
        CreateCode: binary,
        HookOn: '0000000000000000',
    }
    api.prepareTransaction(j).then( (x)=> 
    {
        let s = api.sign(x.txJSON, secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
            process.exit(0);        
        }).catch ( e=> { console.log(e) });
    });
}).then(() => {}).catch(console.error);
