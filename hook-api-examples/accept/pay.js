if (process.argv.length < 5)
{
    console.log("Usage: node pay <source family seed> <amount xrp> <destination account>")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)
const amount = BigInt(process.argv[3]) * 1000000n
const dest = process.argv[4];


const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const api = new RippleAPI({server: 'ws://localhost:6005'});
api.on('error', (errorCode, errorMessage) => {console.log(errorCode + ': ' + errorMessage);});
api.on('connected', () => {console.log('connected');});
api.on('disconnected', (code) => {console.log('disconnected, code:', code);});
api.connect().then(() => {
    let j = {
        Account: address,
        TransactionType: "Payment",
        Amount: "" + amount,
        Destination: dest,
        Fee: "100000"
    }
    api.prepareTransaction(j).then((x)=>
    {
        s = api.sign(x.txJSON, secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
            process.exit(0);
        }).catch (e=> { console.log(e) });
    });
}).then(() => {}).catch(console.error);
