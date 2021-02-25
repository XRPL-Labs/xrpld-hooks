
const process = require('process')
const RippleAPI = require('ripple-lib').RippleAPI;
const bin = require('ripple-binary-codec')
const keypairs = require('ripple-keypairs')
const addr = require('ripple-address-codec')
const {multisignkeys} = require("./multisignkeys.js");

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
api.connect().then(() => {
   


    let proposed_txn = bin.encode(
    {
        TransactionType: "Payment",
        Account: "rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957",
        Destination: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
        Amount: "50000000", // 50 XRP
        LastLedgerSequence: 1000,
        Fee: "10000"
    });

    console.log(proposed_txn)

    let j = {
        Account: 'rJVmjaBshq7jDBLtHvN2D4juayqr6Nk7HL',
        TransactionType: "Payment",
        Amount: "1",
        Destination: "rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957",
        Fee: "100000",
        Memos: [ 
            {
                Memo:{
                    MemoData: proposed_txn,
                    MemoFormat: "unsigned/payload+1",
                    MemoType: "notary/proposed"
                }
            }
        ]
           
    }

    hexlify_memos(j)
    console.log(JSON.stringify(j))

    api.prepareTransaction(j).then((x)=>
    {
        s = api.sign(x.txJSON, 'sn8BAWApiGx86NdFYNVYzK3C1EpyB')
        console.log(s)
        api.submit(s.signedTransaction).then( response => {
            console.log(response.resultCode, response.resultMessage)
            console.log("Done!")
            process.exit()  
        }).catch ( e=> { console.log(e) } );
    });


}).then(() => {
 // return api.disconnect();
}).catch(console.error);
