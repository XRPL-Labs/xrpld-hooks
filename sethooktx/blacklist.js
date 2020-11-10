const process = require('process')
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

/*
Blacklist signing keys
r.deriveKeypair('sEd7CGWXiFPazNncZJsxD11h1sHJCtm')
{
  privateKey: 'ED55D3A139AF8F069FE93BB943FE3A46BAF70EA61E0DD02192D4A532D8E87627F0',
  publicKey: 'EDDC6D9E28CA0FE2D475FC021D226881666EA106FBD2222C8C2110368A49C9513C'
}

Blacklist Account
Address
rNsA4VzfZZydhGAvfHX3gdpcQMMoJafd6v
Secret
ssqTAPTFu7oiJEMPtnhCdYoMeUnQQ
*/
    // first activate the blacklist address
    var activate_nsa = {
        Account: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        TransactionType: "Payment",
        Amount: "10000000000", // 1000 XRP
        Destination: "rNsA4VzfZZydhGAvfHX3gdpcQMMoJafd6v",
        LastLedgerSequence: 20,
        Fee: "100000"
    }

    api.prepareTransaction(activate_nsa).then((x)=> 
    {
        s = api.sign(x.txJSON, 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage);
        
            // then upload the blacklist.wasm hook
            binary = fs.readFileSync('blacklist.wasm').toString('hex').toUpperCase();
            j = {
                Account: 'rNsA4VzfZZydhGAvfHX3gdpcQMMoJafd6v',
                TransactionType: "SetHook",
                CreateCode: binary,
                HookOn: '0000000000000000'
            }
            api.prepareTransaction(j).then((x)=> 
            {
                s = api.sign(x.txJSON, 'ssqTAPTFu7oiJEMPtnhCdYoMeUnQQ')
                console.log(s)
                api.submit(s.signedTransaction).then( response => {
                    console.log(response.resultCode, response.resultMessage);
                    console.log("Done!")
                    process.exit() 
                }).catch ( e=> { console.log(e) });
            });
        }).catch (e=> { console.log(e) });
    });

}).then(() => {
 // return api.disconnect();
}).catch(console.error);
