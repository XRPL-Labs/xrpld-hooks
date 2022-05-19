const fs = require('fs')
const xrpljs = require('xrpl-hooks');
const kp = require('ripple-keypairs');
const crypto = require('crypto')

const rbc = require('xrpl-binary-codec')

const err = (x) =>
{
    console.log(x); process.exit(1);
}
// Fails via process.exit
module.exports = {
    TestRig: (endpoint)=>
    {
        return new Promise((resolve, reject)=>
        {
                const api = new xrpljs.Client(endpoint);

                const fee = (tx_blob) =>
                {
                    return new Promise((resolve, reject) =>
                    {
                        let req = {command: 'fee'};
                        if (tx_blob)
                            req['tx_blob'] = tx_blob;
       
                        api.request(req).then(resp =>
                        {
                            resolve(resp.result.drops);
                        }).catch(e =>
                        {
                            reject(e);
                        });
                    });
                };


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


                const wasm = (x) =>
                {
                    if (x.slice(0,1) != '/')
                        x = 'wasm/' + x;
                    return fs.readFileSync( x).toString('hex').toUpperCase();
                };


                const feeCompute = (account_seed, txn_org) =>
                {
                    return new Promise((resolve, reject) =>
                    {
                        txn_to_send = { ... txn_org };
                        txn_to_send['SigningPubKey'] = '';
                    
                        let wal = xrpljs.Wallet.fromSeed(account_seed);
                        api.prepareTransaction(txn_to_send, {wallet: wal}).then(txn => 
                        {
                            let ser = rbc.encode(txn);
                            fee(ser).then(fees =>
                            {
                                let base_drops = fees.base_fee 
                            
                                delete txn_to_send['SigningPubKey']
                                txn_to_send['Fee'] = base_drops + '';


                                api.prepareTransaction(txn_to_send, {wallet: wal}).then(txn => 
                                {
                                    resolve(txn);
                                }).catch(e=>{reject(e);});
                            }).catch(e=>{reject(e);});
                        }).catch(e=>{reject(e);});
                    });
                }

                const feeSubmit = (seed, txn) =>
                {
                    return new Promise((resolve, reject) =>
                    {
                        feeCompute(seed, txn).then(txn=>
                        {
                            api.submit(txn, 
                                {wallet: xrpljs.Wallet.fromSeed(seed)}).then(s=>
                            {
                                resolve(s);
                            }).catch(e=>{reject(e);});
                        }).catch(e=>{reject(e);});
                    });
                }

                const genesisseed = 'snoPBrXtMeMyMHUVTgbuqAfg1SUTb';
                const genesisaddr = 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh';
                

                const genesis =  xrpljs.Wallet.fromSeed(genesisseed);

                const randomAccount = ()=>
                {
                    const acc = xrpljs.Wallet.fromSeed(kp.generateSeed());
                    console.log(acc)
                    return acc
                };
                
                const pay_mock = (seed, amt, dest) =>
                {
                    if (dest.classicAddress != undefined)
                        dest = dest.classicAddress;

                    return new Promise((resolve, reject) =>
                    {

                        let wal = xrpljs.Wallet.fromSeed(seed);
                        api.prepareTransaction({
                            Account: wal.classicAddress,
                            TransactionType: "Payment",
                            Amount: ''+amt,
                            Destination: dest,
                            SigningPubKey: ''
                        }, {wallet: wal}).then(txn => 
                        {
                            resolve(rbc.encode(txn));
                        }).catch(e=>
                        {
                            reject(e);
                        });
                    });
                            
                }

                const pay = (seed, amt, dest) =>
                {    
                    if (dest.classicAddress != undefined)
                        dest = dest.classicAddress;

                    return new Promise((resolve, reject) =>
                    {
                        let wal = xrpljs.Wallet.fromSeed(seed);

                        api.submit({
                            Account: wal.classicAddress,
                            TransactionType: "Payment",
                            Amount: ''+amt,
                            Destination: dest,
                            Fee: "10000"
                        }, {wallet: wal}).then(x=>
                        {
                            assertTxnSuccess(x);
                            resolve(x);
                        }).catch(err);
                    });
                };

                const hookHash = fn =>
                {
                    let b = fs.readFileSync('wasm/' + fn);
                    return crypto.createHash('SHA512').update(b).digest().slice(0,32).toString('hex').toUpperCase()
                }

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
                            Destination: acc,
                            Fee: "10000"
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
                        rbc: rbc,
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
                        hsfNSDELETE: 2,
                        hfsOVERRIDE: 1,
                        hfsNSDELETE: 2,
                        hookHash: hookHash,
                        pay: pay,
                        pay_mock: pay_mock,
                        fee: fee,
                        genesisseed: genesisseed,
                        genesisaddr: genesisaddr,
                        feeCompute: feeCompute,
                        feeSubmit: feeSubmit
                    });
                }).catch(err);
        });
    }
};
