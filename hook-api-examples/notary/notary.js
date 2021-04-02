if (process.argv.length < 5)
{
    console.log("Usage: node notary <source family seed> <quorum> <signer 1> < ... signer n> ")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)

const quorum = parseInt(process.argv[3]);

let signers = [];
for (let i = 4; i < process.argv.length; ++i)
{
    signers.push({
        SignerEntry: {
            Account: process.argv[i],
            SignerWeight: 1
        }
    });
}

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
    // set signer list 
    s = {
        Account: address,
        Flags: 0,
        TransactionType: "SignerListSet",
        SignerQuorum: quorum,
        SignerEntries: signers
    };

    api.prepareTransaction(s).then((x)=>{
        s = api.sign(x.txJSON, secret)
        api.submit(s.signedTransaction).then( response => {
            console.log('SignerListSet:', response.resultCode, response.resultMessage);
            binary = fs.readFileSync('notary.wasm').toString('hex').toUpperCase();
            j = {
                Account: address,
                TransactionType: "SetHook",
                CreateCode: binary,
                HookOn: '0000000000000000'
            }
            api.prepareTransaction(j).then((x)=> 
            {
                s = api.sign(x.txJSON, secret)
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage);
                    process.exit(0)
                }).catch ( e=> { console.log(e) });
            });
        }).catch ( e=> { console.log(e) });
    }).catch ( e=> { console.log(e) });
}).then(() => {}).catch(console.error);
