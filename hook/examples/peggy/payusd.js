if (process.argv.length < 5)
{
    console.log("Usage: node payusd <family seed> <amount> <peggy hook account> [destination] [dest tag]")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)
const amount = process.argv[3]
const hook_account = process.argv[4];

const dest_acc = (process.argv.length >= 6 ? process.argv[5] : hook_account);
const dest_tag = (process.argv.length == 7 ? process.argv[6] : null);
require('../../utils-tests.js').TestRig('ws://localhost:6005').then(t=>
{
    t.feeSubmit(secret, 
    {
        Account: address,
        TransactionType: "Payment",
        Amount: {
            currency: "USD",
            value: amount + '',
            issuer: hook_account
        },
        Destination: dest_acc
    }).then(x=>
    {
        console.log(x);
        process.exit(0);
    });
});


