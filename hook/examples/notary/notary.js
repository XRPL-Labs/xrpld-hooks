const wasm = 'notary.wasm'
if (process.argv.length < 5)
{
    console.log("Usage: node notary <source family seed> <quorum> <signer 1> < ... signer n> ")
    process.exit(1)
}

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


require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{

    const account = t.xrpljs.Wallet.fromSeed(process.argv[2]);
    t.fundFromGenesis([account.classicAddress]).then(x=>
    {
        t.feeSubmit(process.argv[2],
        {
            Account: account.classicAddress,
            Flags: 0,
            TransactionType: "SignerListSet",
            SignerQuorum: quorum,
            SignerEntries: signers
        }).then(x=>
        {
            console.log(x);
            t.assertTxnSuccess(x);
            const secret  = process.argv[2];
            const account = t.xrpljs.Wallet.fromSeed(secret)
            t.feeSubmit(process.argv[2],
            {
                Account: account.classicAddress,
                TransactionType: "SetHook",
                Hooks: [
                    {
                        Hook: {
                            CreateCode: t.wasm(wasm),
                            HookApiVersion: 0,
                            HookNamespace: "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE",
                            HookOn: "0000000000000000",
                            Flags: t.hsfOVERRIDE
                        }
                    }
                ]
            }).then(x=>
            {
                t.assertTxnSuccess(x)
                console.log(x);
                process.exit(0);

            }).catch(t.err);
        }).catch(t.err);
    }).catch(t.err);
}).catch(e=>console.log(e));
