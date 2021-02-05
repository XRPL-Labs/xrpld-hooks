const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const fs = require('fs');
const acc = JSON.parse(fs.readFileSync('/root/testnet-keys.json').toString('utf-8'));
const api = new RippleAPI({
    server: 'ws://localhost:6005'
});

function hexlify_memos(x)
{
    if (!("Memos" in x))
        return;

    for (y in x["Memos"])
    {
        for (a in x["Memos"][y])
        {
            let Fields = ["MemoFormat", "MemoType", "MemoData"];
            for (z in Fields)
            {
                if (Fields[z] in x["Memos"][y][a])
                {
                    let u = x["Memos"][y][a][Fields[z]].toUpperCase()
                    if (u.match(/^[0-9A-F]+$/))
                    {
                        x["Memos"][y][a][Fields[z]] = u;
                        continue;
                    }

                    x["Memos"][y][a][Fields[z]] =
                            ""+Buffer.from(x["Memos"][y][a][Fields[z]]).toString('hex').toUpperCase();
                }
            }
        }
    }
}
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
        Account: acc.friend.raddr,
        TransactionType: "Payment",
        Amount: "1", // 1 drop
        Destination: acc.daddy.raddr,
        Fee: "100",
        Memos: [
        {
            Memo:{
                MemoData: acc.alice.raddr,
                MemoFormat: "unsigned/payload+1",
                MemoType: "faucetreq"
            }
        }]
    }
    hexlify_memos(j);
    api.prepareTransaction(j).then( (x)=>
    {
        s = api.sign(x.txJSON, acc.friend.secret)
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Done!")
            process.exit()
        }).catch ( e=> { console.log(e) } );
     });
}).then(() => {
}).catch(console.error);
