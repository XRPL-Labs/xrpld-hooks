const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const acc = JSON.parse(fs.readFileSync('/root/testnet-keys.json').toString('utf-8'));

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
        Account: acc.genesis.raddr,
        TransactionType: "Payment",
        Amount: "10000000000000000", // 10 billion xrp
        Destination: acc.daddy.raddr,
        Fee: "100"
    }
    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, acc.genesis.secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Done!")


            binary = fs.readFileSync('faucet.wasm').toString('hex').toUpperCase();
            j = {
                Account: acc.daddy.raddr,
                TransactionType: "SetHook",
                CreateCode: binary,
                HookOn: '0000000000000000'
            }
            api.prepareTransaction(j).then( (x)=>
            {
                let s = api.sign(x.txJSON, acc.daddy.secret)
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage);
                    j = {
                        Account: acc.genesis.raddr,
                        TransactionType: "Payment",
                        Amount: "1000000000000", // 1 million xrp
                        Destination: acc.friend.raddr,
                        Fee: "100"
                    }
                    api.prepareTransaction(j).then( (x)=>
                    {
                        s = api.sign(x.txJSON, acc.genesis.secret)
                        console.log(s)
                        api.submit(s.signedTransaction).then( response => {
                            console.log(response.resultCode, response.resultMessage)
                            console.log("Done!")
                            process.exit()
                        }).catch ( e=> { console.log(e) } );
                    });
                }).catch ( e=> { console.log(e) });
            });
        }).catch ( e=> { console.log(e) } );
    });
}).then(() => {
}).catch(console.error);
