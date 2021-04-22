if (process.argv.length < 4)
{
    console.log("Usage: node addtrustline <family seed> <peggy hook account> ")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)
const hook_account = process.argv[3];

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
        Account: address,
        TransactionType: "TrustSet",
        Flags: 262144,
        LimitAmount: {
            currency: "USD",
            issuer: hook_account,
            value: "1000000000000000"
        },
        Fee: "10000"
    }
    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            process.exit()
        }).catch ( e=> { console.log(e) } );
     });
}).then(() => {}).catch(console.error);
