if (process.argv.length < 4)
{
    console.log("Usage: node firewall <source family seed> <blacklist hook account>")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)
const blacklistacc = process.argv[3];

const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const ccode = fs.readFileSync('firewall.c').toString('utf-8');
if (!ccode.match(blacklistacc))
{
    console.log("You need to update firewall.c to point at blacklist acc: " + process.argv[3]);
    process.exit(1);
}

const binary = fs.readFileSync('firewall.wasm').toString('hex').toUpperCase();
if (!binary.match(Buffer.from(blacklistacc, 'utf-8').toString('hex').toUpperCase()))
{
    console.log("You need to recompile firewall.c to point at blacklist acc: " + process.argv[3]);
    process.exit(1)
}

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
        TransactionType: "SetHook",
        CreateCode: binary,
        HookOn: '0000000000000000'
    }
    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, secret);
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
            process.exit(0);
        }).catch ( e=> { console.log(e) });
    });
}).then(() => {}).catch(console.error);
