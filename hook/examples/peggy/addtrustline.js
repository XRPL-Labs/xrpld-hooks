if (process.argv.length < 4)
{
    console.log("Usage: node addtrustline <family seed> <peggy hook account> ")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)
const hook_account = process.argv[3];

require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    t.feeSubmit(secret, 
    {
        Account: address,
        TransactionType: "TrustSet",
        Flags: 262144,
        LimitAmount: {
            currency: "USD",
            issuer: hook_account,
            value: "1000000000000000"
        },
    }).then(x=>
    {
        console.log(x);
        t.assertTxnSuccess(x);
        process.exit(0);
    });
});


