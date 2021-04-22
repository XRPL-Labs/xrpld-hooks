if (process.argv.length < 5)
{
    console.log("Usage: node payxrp <family seed> <xrp amount> <peggy hook account> [destination] [dest tag]")
    process.exit(1)
}
const keypairs = require('ripple-keypairs');
const secret  = process.argv[2];
const address = keypairs.deriveAddress(keypairs.deriveKeypair(secret).publicKey)
const amount = BigInt(process.argv[3]) * 1000000n;
const hook_account = process.argv[4];

const dest_acc = (process.argv.length >= 6 ? process.argv[5] : hook_account);
const dest_tag = (process.argv.length == 7 ? process.argv[6] : null);

const RippleAPI = require('ripple-lib').RippleAPI;
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
        Amount: amount + '', 
        Destination: dest_acc,
        Fee: "10000"
    }

    if (dest_tag !== null)
        j['DestinationTag'] = dest_tag;

    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            if (dest_acc != hook_account)
                process.exit(0);
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
