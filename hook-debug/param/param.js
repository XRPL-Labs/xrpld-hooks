if (process.argv.length < 3)
{
    console.log("Usage: node param <account family seed>")
    process.exit(1);
}
const RippleAPI = require('ripple-lib').RippleAPI;
const keypairs = require('ripple-keypairs');
const addr = require('ripple-address-codec')
const fs = require('fs');
const api = new RippleAPI({server: 'ws://localhost:6005'});

const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)

api.on('error', (errorCode, errorMessage) => {console.log(errorCode + ': ' + errorMessage);});
api.on('connected', () => {console.log('connected');});
api.on('disconnected', (code) => {console.log('disconnected, code:', code);});
api.connect().then(() => {
    j = {
        Account: address,
        TransactionType: "SetHook",
        Hooks:
        [
            {
                Hook: {
                    CreateCode: fs.readFileSync('param.wasm').toString('hex').toUpperCase(),
                    HookOn: '0000000000000000',
                    HookNamespace: addr.codec.sha256('param').toString('hex').toUpperCase(),
                    HookApiVersion: 0,
                    HookParameters:
                    [
                        {
                            HookParameter:
                            {
                                HookParameterName: "DEADBEEF",
                                HookParameterValue: "CAFEBABE"
                            }
                        }
                    ]
                }
            }
        ]
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
