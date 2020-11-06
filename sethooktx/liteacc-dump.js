const hook_account = 'rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957'
const exec = require('child_process').exec
const crypto = require('crypto')
const addr = require('ripple-address-codec')

//8E837C8A48B398D9506C5779BE38BBE284050A5B083A5F4D097E4D7BB40137C9
function sha512h(x)
{
    var hash = crypto.createHash('sha512')
    return hash.update(x).digest('hex').slice(0, 64).toUpperCase()
}


function hook_state_keylet(accountid, statekey)
{
    accountid = addr.decodeAccountID(accountid)
    if (typeof(statekey) == 'string')
        statekey = Buffer.from(statekey, 'hex');
    return sha512h(Buffer.concat([Buffer.from([0x00, 0x76]), accountid, statekey]))
}


function make_special_buf(front, end)
{
    var b = Buffer.from("0000000000000000000000000000000000000000000000000000000000000000", 'hex');
    b[28] = (end   >> 24) & 0xFF;
    b[29] = (end   >> 16) & 0xFF;
    b[30] = (end   >>  8) & 0xFF;
    b[31] = (end   >>  0) & 0xFF;
    b[0]  = (front >> 24) & 0xFF;
    b[1]  = (front >> 16) & 0xFF;
    b[2]  = (front >>  8) & 0xFF;
    b[3]  = (front >>  0) & 0xFF;
    return b;
}

function uint32_from_buf(b)
{
    if (typeof(b) == 'string') b = Buffer.from(b, 'hex')
    return  (BigInt(b[0]) << 24n) +
            (BigInt(b[1]) << 16n) +
            (BigInt(b[2]) <<  8n) +
            (BigInt(b[3]) <<  0n);
}

function uint64_from_buf(b)
{
    if (typeof(b) == 'string') b = Buffer.from(b, 'hex')
    return  (BigInt(b[0]) << 56n) +
            (BigInt(b[1]) << 48n) +
            (BigInt(b[2]) << 40n) +
            (BigInt(b[3]) << 32n) +
            (BigInt(b[4]) << 24n) +
            (BigInt(b[5]) << 16n) +
            (BigInt(b[6]) <<  8n) +
            (BigInt(b[7]) <<  0n);
}

next_tag_lookup = hook_state_keylet(hook_account, make_special_buf(0, 0))

// seq keylet -> userid, used to lookup sequence number
seq_lookup = {}
// user tag keylet -> user tag, used to lookup public key
pub_lookup = {}

for (let i = 1; i < 10; ++i)
{
    seq_lookup[hook_state_keylet(hook_account, make_special_buf(0xFFFFFFFF, i))] = i
    pub_lookup[hook_state_keylet(hook_account, make_special_buf(0, i))] = i
}

lite_accounts = {}

exec('./rippled account_objects ' + hook_account, (err, stdout, stderr)=>
{
    let j = JSON.parse(stdout)
    if (!('result' in j))
        return console.log("no JSON result returned")
    j = j['result']

    if (!('account_objects' in j))
        return console.log("no account_objects returned")
    j = j['account_objects']


    // pass 1
    for (x in j)
    {
        if ('CreateCode' in j[x])
            j[x]['CreateCode'] = '<truncated>';

        if ('index' in j[x])
        {
            let ind = j[x]['index']
            if (ind in seq_lookup)
            {
                let userid = 'Tag_' + seq_lookup[ind];
                j[x]['<LITEACC_TYPE>'] = 'Sequence Number';
                j[x]['<LITEACC_USER>'] = userid;
                if (!(userid in lite_accounts))
                    lite_accounts[ userid] = {}
                lite_accounts[ userid]['Sequence'] = j[x]['HookData']
            } else if (ind in pub_lookup)
            {
                let userid = 'Tag_' + pub_lookup[ind];
                j[x]['<LITEACC_TYPE>'] = 'Public Key';
                j[x]['<LITEACC_USER>'] = userid;
                if (!(userid in lite_accounts))
                    lite_accounts[ userid] = {}
                lite_accounts[ userid]['PublicKey'] = j[x]['HookData']
            }
        }
    }

    // public key keylet -> userid, used to lookup balance
    bal_lookup = {}
    for (x in lite_accounts)
        bal_lookup[hook_state_keylet(hook_account, lite_accounts[x]['PublicKey'])] = x


    // pass 2
    for (x in j)
    {
        if ('index' in j[x])
        {
            let ind = j[x]['index']
            for (pk in bal_lookup)
            {
                if (ind == pk)
                {
                    lite_accounts[bal_lookup[pk]]['Balance'] = uint64_from_buf(j[x]['HookData']);
                    j[x]['<LITEACC_TYPE>'] = 'Balance';
                    j[x]['<LITEACC_USER>'] = bal_lookup[pk];
                }
            }
            if (ind == next_tag_lookup)
                lite_accounts['Next_Tag'] = uint32_from_buf(j[x]['HookData']) + 1n;
        }
    }

    console.log("")
    console.log("----------- CONTRACT STATE --------------")
    console.log("")
    console.log(j);

    console.log("")
    console.log("----------- TRANSLATED TABLE ------------")
    console.log("")
    console.log(lite_accounts);
});


//console.log(hook_state_keylet("rGGLq3bp1oMjzFwwXnt3kMVtqgKpcue957", "0000000000000000000000000000000000000000000000000000000000000001"))
