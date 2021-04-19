if (process.argv.length < 4)
{
    console.log("Usage: node balance <account> <hook account>")
    process.exit(1)
}
const {xls17StringDec} = require('./xls17.js') 
const codec = require('ripple-address-codec');
const address = process.argv[2];
const hook_account = process.argv[3];
let accid = codec.decodeAccountID(address).toString('hex').toUpperCase();
if (accid.length < 40) accid = '0'.repeat(40 - accid.length) + accid;

let statekey = "0".repeat(16) + accid + 'FFFFFFFF';


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

if (process.argv.length < 3)
{
    console.log("Must provide txn hash as cmdline argument");
    process.exit(1);
}

api.connect().then(() => {

    api.getAccountObjects(hook_account).then(x=>{
        let objs = x.account_objects;
        for (x in objs)
        {
            if (objs[x].LedgerEntryType == 'HookState')
            {
                if (objs[x].HookStateKey == statekey)
                {
                    // found!
                    let data = objs[x].HookStateData;
                    let f1 = data.slice(0,16);
                    let f2 = data.slice(16);

                    console.log("PUSD: ", xls17StringDec(f1), "XRP: ", xls17StringDec(f2))
                    process.exit(0);
                }
            }
        }
        console.log("No balance found")
    });
});
