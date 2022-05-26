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

                const ledgerAccept = () =>
                {
                    return new Promise((resolve, reject) =>
                    {
                        let req = {command: 'ledger_accept'};
                        api.request(req).then(resp =>
                        {
                            resolve(resp)
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

                const assert = (x, m) =>
                {
                    if (!(x))
                    {
                        console.log("Assertion failed: ", m);
                        console.log(new Error().stack);
                        process.exit(1);
                    }
                };

                const fetchMeta = (hash) =>
                {
                    if (typeof(hash) != 'string')
                        hash = hash.result.tx_json.hash
                    
                    return new Promise((resolve, reject) =>
                    {
                        api.request(
                        {
                            command:"tx", 
                            transaction: hash
                        }).then(e=>{
                            resolve(e.result.meta)
                        }).catch(e=>reject(e));
                    });
                };
                

                const fetchMetaHookExecutions = (hash, hookhash) =>
                {
                    return new Promise((resolve, reject) =>
                    {
                        fetchMeta(hash).then(m=>
                        {
                            if (typeof(m) == 'undefined' ||
                                typeof(m.HookExecutions) == 'undefined' ||
                                typeof(m.HookExecutions.length) == 'undefined')
                                    reject(m);

                            let ret = [];

                            for (let i = 0; i < m.HookExecutions.length; ++i)
                            {
                                if (typeof(hookhash) == 'undefined' ||
                                    m.HookExecutions[i].HookExecution.HookHash == hookhash)
                                m.HookExecutions[i].HookExecution.HookReturnCode =
                                    parseInt(m.HookExecutions[i].HookExecution.HookReturnCode, 16);
                                m.HookExecutions[i].HookExecution.HookInstructionCount =
                                    parseInt(m.HookExecutions[i].HookExecution.HookInstructionCount, 16);

                                ret.push(m.HookExecutions[i].HookExecution);
                            }

                            resolve(ret);
                        }).catch(e=>reject(e));
                    });
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


                const wasmHash = (x)=>
                {
                    const blob = wasm(x);
                    return crypto.createHash('SHA512').
                        update(Buffer.from(blob, 'hex')).
                        digest().slice(0,32).toString('hex').toUpperCase();
                }

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

                                api.request(
                                {
                                    command: "account_info",
                                    account: txn.Account
                                }).then(y=>
                                {
                                    let seq = (y.result.account_data.Sequence);
                                    txn_to_send.Sequence = seq;
                                    api.prepareTransaction(txn_to_send, {wallet: wal}).then(txn => 
                                    {
                                        resolve(txn);
                                    }).catch(e=>{reject(e);});
                                }).catch(e=>{reject(e);});
                            }).catch(e=>{reject(e);});
                        }).catch(e=>{reject(e);});
                    });
                }

                const feeSubmitAccept = (seed, txn) =>
                {
                    return new Promise((resolve, reject) => 
                    {
                        feeSubmit(seed, txn).then(x=>
                        {
                            ledgerAccept().then(()=>
                            {
                                resolve(x);
                            }).catch(e=>
                            {
                                reject(e);
                            });
                        }).catch(e =>
                        {
                            reject(e);
                        });
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
                        const ffg = (acc, after) =>
                        {
                            if (typeof(acc) != 'string')
                                acc = acc.classicAddress;

                            console.log('ffg: ' + acc);
                            feeSubmitAccept(genesis.seed, {
                                Account: genesis.classicAddress,        // fund account from genesis
                                TransactionType: "Payment",
                                Amount: "1000000000",
                                Destination: acc,
                            }).then(x=>
                            {
                                assertTxnSuccess(x);
                                if (after)
                                    return after();
                                else
                                    resolve();
                            }).catch(err);
                        };

                        const doFfg = (acc) =>
                        {

                            if (typeof(acc.length) == 'undefined')
                                return ffg(acc);
                            else if (acc.length == 1)
                                return ffg(acc[0]);
                            else
                            {
                                return ffg(acc[0],
                                    ((acc)=>{
                                        return ()=>{
                                            acc.shift();
                                            return doFfg(acc);
                                        };
                                    })(acc));
                            }
                        }

                        return doFfg(acc);

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
                        hsfCOLLECT: 4,
                        asfTshCollect: 11,
                        hookHash: hookHash,
                        pay: pay,
                        pay_mock: pay_mock,
                        fee: fee,
                        genesisseed: genesisseed,
                        genesisaddr: genesisaddr,
                        feeCompute: feeCompute,
                        feeSubmit: feeSubmit,
                        feeSubmitAccept: feeSubmitAccept,
                        ledgerAccept: ledgerAccept,
                        fetchMeta: fetchMeta,
                        fetchMetaHookExecutions: fetchMetaHookExecutions,
                        wasmHash: wasmHash,
                        assert: assert
                    });
                }).catch(err);
        });
    }
};
