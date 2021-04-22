if (process.argv.length < 6)
{
    console.log("Usage: node pay-to-lite <source family seed> <amount xrp> <lite hook account> <lite acc key or dtag>")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)
const amount = BigInt(process.argv[3]) * 1000000n
const hook_account = process.argv[4];
const liteacc = process.argv[5];
const is_tag = (!!liteacc.match(/^[0-9]+$/));

const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const addr = require('ripple-address-codec')

const fs = require('fs');
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
        TransactionType: "Payment",
        Amount: '' + amount,
        Destination: hook_account,
        Fee: "10000"
    }

    if (is_tag)
        j['DestinationTag'] = parseInt(liteacc);
    else
        j['InvoiceID'] = liteacc.slice(2);

    api.prepareTransaction(j).then((x)=>
    {
        s = api.sign(x.txJSON, secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Waiting for next ledger to get hook result...")
            let countdown = (x)=>{
                if (x <= 0)
                    return console.log("")
                process.stdout.write(x + "... ");
                setTimeout(((x)=>{ return ()=>{countdown(x);} })(x-1), 1000);
            };
            countdown(6);
            setTimeout(
            ((txnhash)=>{
                return ()=>{
                    api.getTransaction(txnhash, {includeRawTransaction: true}).then(
                        x=>{
                            execs = JSON.parse(x.rawTransaction).meta.HookExecutions;
                            for (y in execs)
                            {
                                exec = execs[y].HookExecution;
                                if (exec.HookAccount == hook_account)
                                {

                                    console.log("Hook Returned: ", 
                                        Buffer.from(exec.HookReturnString, 'hex').toString('utf-8'));
                                    process.exit(0);
                                }
                            }
                            console.log("Could not find return from hook");
                            process.exit(1);
                        });
                }
            })(s.id), 6000);
        }).catch ( e=> { console.log(e) } );
    });

}).then(() => {}).catch(console.error);

