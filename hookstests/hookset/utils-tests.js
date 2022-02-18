const fs = require('fs')
const xrpljs = require('xrpl-hooks');
const kp = require('ripple-keypairs');

// Fails via process.exit
module.exports = {
    TestRig: (endpoint)=>
    {
        return new Promise((resolve, reject)=>
        {
            const api = new xrpljs.Client(endpoint);

            const assertTxnSuccess = x =>
            {
                if (!x || !x.result || x.result.engine_result_code != 0)
                {
                    console.log("Transaction failed:", x)
                    process.exit(1);
                }
            };

            const assertTxnFailure = x =>
            {
                if (!x || !x.result || x.result.engine_result_code == 0)
                {
                    console.log("Transaction failed:", x)
                    process.exit(1);
                }
            };

            const err = (x) =>
            {
                console.log(x); process.exit(1);
            }

            const wasm = (x) =>
            {
                return fs.readFileSync('wasm/' + x).toString('hex').toUpperCase();
            }

            const genesis =  xrpljs.Wallet.fromSeed('snoPBrXtMeMyMHUVTgbuqAfg1SUTb');

            const randomAccount = ()=>
            {
                return xrpljs.Wallet.fromSeed(kp.generateSeed());
            };

            const fundFromGenesis = (acc) =>
            {    
                return new Promise((resolve, reject) =>
                {
                    if (typeof(acc) != 'string')
                        acc = acc.classicAddress;

                    api.submit({
                        Account: genesis.classicAddress,        // fund account from genesis
                        TransactionType: "Payment",
                        Amount: "1000000000",
                        Destination: acc
                    }, {wallet: genesis}).then(x=>
                    {
                        assertTxnSuccess(x);
                        resolve();
                    }).catch(err);
                });
            };

            api.connect().then(()=>
            {
                resolve({
                    api: api,
                    xrpljs: xrpljs,
                    assertTxnSuccess: assertTxnSuccess,
                    assertTxnFailure: assertTxnFailure,
                    wasm: wasm,
                    kp: kp,
                    genesis: genesis,
                    randomAccount: randomAccount,
                    fundFromGenesis: fundFromGenesis,
                    err: err,
                    hsfOVERRIDE: 1,
                    hsfNSDELETE: 2
                });
            }).catch(err);
        });
    }
};
