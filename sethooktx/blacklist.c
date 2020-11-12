#include "hookapi.h"

// this key is used by the blacklist hook to check if an add/remove instruction is legitimate
#define ADMIN_PUBLIC_KEY {0xED, 0xDC, 0x6D, 0x9E, 0x28, 0xCA, 0x0F, 0xE2, 0xD4, 0x75, 0xFC, 0x02, 0x1D, 0x22, 0x68,\
         0x81, 0x66, 0x6E, 0xA1, 0x06, 0xFB, 0xD2, 0x22, 0x2C, 0x8C, 0x21, 0x10, 0x36, 0x8A, 0x49, 0xC9, 0x51, 0x3C}
    
// this hook has no emitted tx and therefore no callbacks
int64_t cbak(int64_t reserved)
{
    return 0;
}

int64_t hook(int64_t reserved )
{

    GUARD(1);

    // this api fetches the AccountID of the account the hook currently executing is installed on
    // since hooks can be triggered by both incoming and ougoing transactions this is important to know
    unsigned char hook_accid[20];
    hook_account((uint32_t)hook_accid, 20);

    // next fetch the sfAccount field from the originating transaction
    uint8_t account_field[20];
    int32_t account_field_len = otxn_field(SBUF(account_field), sfAccount);
    if (account_field_len < 20)                                   // negative values indicate errors from every api
        rollback(SBUF("Blacklist: sfAccount field missing!!!"), 10); // this code could never be hit in prod
                                                                  // but it's here for completeness
    // compare the "From Account" (sfAccount) on the transaction with the account the hook is running on
    int equal = 0; BUFFER_EQUAL(equal, hook_accid, account_field, 20);
    if (equal)
        accept(SBUF("Blacklist: Passing outgoing transaction"), 0);

    // check for the presence of a memo
    uint8_t memos[2048];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);

    uint32_t payload_len = 0, signature_len = 0, publickey_len = 0;
    uint8_t* payload_ptr = 0, *signature_ptr = 0, *publickey_ptr = 0;
    if (memos_len <= 0)
       accept(SBUF("Blacklist: Passing non-memo incoming transaction."), 0);
        
    /**
     * 'Signed Memos' for hooks are supplied in triples in the following 'default' format as per XLS-14d:
     * NB: The +1 identifies the payload, you may provide multiple payloads
     * Memo: { MemoData: <app data>,   MemoFormat: "signed/payload+1",   MemoType: [application defined] }
     * Memo: { MemoData: <signature>,  MemoFormat: "signed/signature+1", MemoType: [application defined] }
     * Memo: { MemoData: <public_key>, MemoFormat: "signed/publickey+1", MemoType: [application defined] }
     **/

    // loop through the three memos (if 3 are even present) to parse out the relevant fields
    for (int i = 0; GUARD(3), i < 3; ++i)
    {
        // the memos are presented in an array object, which we must index into
        int64_t memo_lookup = util_subarray(memos, memos_len, i);

        TRACEVAR(memo_lookup);
        if (memo_lookup < 0)
            rollback(SBUF("Blacklist: Memo transaction did not contain XLS14 format."), 30);

        // if the subfield/array lookup is successful we must extract the two pieces of returned data
        // which are, respectively, the offset at which the field occurs and the field's length
        uint8_t*  memo_ptr = SUB_OFFSET(memo_lookup) + memos;
        uint32_t  memo_len = SUB_LENGTH(memo_lookup);

        trace(SBUF("MEMO:"), 0);
        trace(memo_ptr, memo_len, 1);

        // memos are nested inside an actual memo object, so we need to subfield
        // equivalently in JSON this would look like memo_array[i]["Memo"]
        memo_lookup = util_subfield(memo_ptr, memo_len, sfMemo);
        memo_ptr = SUB_OFFSET(memo_lookup) + memo_ptr;
        memo_len = SUB_LENGTH(memo_lookup);

        // now we lookup the subfields of the memo itself
        // again, equivalently this would look like memo_array[i]["Memo"]["MemoData"], ... etc.
        int64_t data_lookup = util_subfield(memo_ptr, memo_len, sfMemoData);
        int64_t format_lookup = util_subfield(memo_ptr, memo_len, sfMemoFormat);

        // if any of these lookups fail the request is malformed
        if (data_lookup < 0 || format_lookup < 0)
            rollback(SBUF("Blacklist: Memo transaction did not contain XLS14 format."), 40);

        // care must be taken to add the correct pointer to an offset returned by sub_array or sub_field
        // since we are working relative to the specific memo we must add memo_ptr, NOT memos or something else
        uint8_t* data_ptr = SUB_OFFSET(data_lookup) + memo_ptr;
        uint32_t data_len = SUB_LENGTH(data_lookup);

        uint8_t* format_ptr = SUB_OFFSET(format_lookup) + memo_ptr;
        uint32_t format_len = SUB_LENGTH(format_lookup);

        // we can use a helper macro to compare the format fields and determine which MemoData is assigned
        // to each pointer. Note that the last parameter here tells the macro how many times we will hit this
        // line so it in turn can correctly configure its GUARD(), otherwise we will get a guard violation
        int is_payload = 0, is_signature = 0, is_publickey = 0;
        BUFFER_EQUAL_STR_GUARD(is_payload, format_ptr, format_len,   "signed/payload+1", 3);
        BUFFER_EQUAL_STR_GUARD(is_signature, format_ptr, format_len, "signed/signature+1", 3);
        BUFFER_EQUAL_STR_GUARD(is_publickey, format_ptr, format_len, "signed/publickey+1", 3);

        // assign the pointers according to the detected MemoFormat
        if (is_payload)
        {
            payload_ptr = data_ptr;
            payload_len = data_len;
        } else if (is_signature)
        {
            signature_ptr = data_ptr;
            signature_len = data_len;
        } else if (is_publickey)
        {
            publickey_ptr = data_ptr;
            publickey_len = data_len;
        }
    }

    if (!(payload_ptr && signature_ptr && publickey_ptr))
        rollback(SBUF("Blacklist: Memo transaction did not contain XLS14 format."), 50);

    // check the public key is correct
    if (publickey_len != 33)
        rollback(SBUF("Blacklist: Memo public key wrong length."), 55);

    uint8_t blacklist_key[33] = ADMIN_PUBLIC_KEY;
    equal = 0;
    BUFFER_EQUAL(equal, blacklist_key, publickey_ptr, 33);
    if (!equal)
        rollback(SBUF("Blacklist: Invalid admin public key."), 57);

    // check the signature is valid
    if (!util_verify(payload_ptr,    payload_len,
                     signature_ptr,  signature_len,
                     blacklist_key,  33))
        rollback(SBUF("Blacklist: Invalid signature in memo."), 60);

    // execution to here means that BUFFER<payload_ptr,payload_len> contains a validly signed object
    // now check if it is properly constructed
    // the expected format is a generic STObject containing
    // - at least:      sfFlags     sfSequence      sfTemplate(ARRAY){sfAccount}
    // Flags 0 means add and Flags 1 means remove
    // Sequence must be greater than the previously used Sequence (timestamp is desirable but not mandated)
    // Sequence prevents replay attacks
    // ARRAY must contain at least one sfAccount

    int64_t lookup_flags    = util_subfield(payload_ptr, payload_len, sfFlags);
    int64_t lookup_seq      = util_subfield(payload_ptr, payload_len, sfSequence);
    int64_t lookup_array    = util_subfield(payload_ptr, payload_len, sfTemplate);

    TRACEVAR(lookup_flags);
    TRACEVAR(lookup_seq);
    TRACEVAR(lookup_array);

    if (lookup_seq < 0 || lookup_flags < 0 || lookup_array < 0)
        rollback(SBUF("Blacklist: Validly signed memo lacked required STObject fields."), 70);

    // extract the actual transaction details, again taking care to add the correct pointer to the offset
    uint32_t seq   = UINT32_FROM_BUF(SUB_OFFSET(lookup_seq)   + payload_ptr);
    uint32_t flags = UINT32_FROM_BUF(SUB_OFFSET(lookup_flags) + payload_ptr);
    uint8_t* array_ptr = SUB_OFFSET(lookup_array) + payload_ptr;
    int array_len = SUB_LENGTH(lookup_array);
   
    // get the previous sequence number from the hook state (this is the 0 key)
    uint8_t state_request[32];
    uint8_t seq_buffer[4];
    CLEARBUF(state_request);
    if (state(SBUF(seq_buffer), SBUF(state_request)) != 4)
    {
        // first run
    } else
    {
        if (seq <= UINT32_FROM_BUF(seq_buffer))
            rollback(SBUF("Blacklist: Sequence number was less than previous sequence."), 75);
    }

    // update sequence number
    UINT32_TO_BUF(seq_buffer, seq);
    if (state_set(SBUF(seq_buffer), SBUF(state_request)) != 4)
        rollback(SBUF("Blacklist: Sequence number could not be updated."), 77);

    // we will accept at most 50 accounts in the array
    int processed_count = 0;
    for (int i = 0; GUARD(50), i < 50; ++i)
    {
        int64_t lookup_array_entry = util_subarray(array_ptr, array_len, i);

        TRACEVAR(lookup_array_entry);
        if (lookup_array_entry < 0)
            break; // ran out of array entries to process

        uint8_t* array_entry_ptr = SUB_OFFSET(lookup_array_entry) + array_ptr;
        uint32_t array_entry_len = SUB_LENGTH(lookup_array_entry);

        // this will return the actual payload inside the sfAccount inside the array entry
        int64_t lookup_acc = util_subfield(array_entry_ptr, array_entry_len, sfAccount);
        if (lookup_acc < 0)
            rollback(SBUF("Blacklist: Invalid array entry, expecting sfAccount."), 80);

        uint8_t* acc_ptr = SUB_OFFSET(lookup_acc) + array_entry_ptr;
        uint32_t acc_len = SUB_LENGTH(lookup_acc);

        if (acc_len != 20)
            rollback(SBUF("Blacklist: Invalid sfAccount, expecting length = 20."), 90);


        uint8_t buffer[1] = {1}; // nominally we will simply a store a single byte = 1 for a blacklisted account
        uint32_t len = flags == 1 ? 1 : 0; // we will pass length = 0 to state_set for a delete operation
        if (state_set(buffer, len, acc_ptr, acc_len) == len)
        {
            processed_count++;
        } else
        {
            trace(SBUF("Blacklist: Failed to update state for the following account."), 0);
            trace(acc_ptr, acc_len, 1);
        }
    }

    RBUF(result_buffer, result_len, "Blacklist: Processed + ", processed_count);
    if (flags == 0) 
        result_buffer[21] = '-';
    accept(result_buffer, result_len, 0); 

    return 0;
}
