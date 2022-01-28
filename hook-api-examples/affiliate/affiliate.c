#include <stdint.h>
#include "../hookapi.h"



int64_t cbak(int64_t reserved)
{
    return 0;
}



int64_t hook(int64_t reserved ) {

    TRACESTR("Affiliate: started");

    etxn_reserve(2); // we are going to emit two transactions


    // get memos
    uint8_t memos[2048];
    int64_t memos_len = otxn_field(SBUF(memos), sfMemos);
    
    if (memos_len <= 0)
       accept(SBUF("No memo. Regular transaction."), 0);


    // vars for storing memo data
    uint32_t payload_len = 0, signature_len = 0, publickey_len = 0;
    uint8_t* payload_ptr = 0, *signature_ptr = 0, *publickey_ptr = 0;


    // getting memo data
    for (int i = 0; GUARD(3), i < 3; ++i)
    {
        // the memos are presented in an array object, which we must index into
        int64_t memo_lookup = sto_subarray(memos, memos_len, i);

        if (memo_lookup < 0)
            rollback(SBUF("Memo transaction did not contain correct format."), 30);


        // if the subfield/array lookup is successful we must extract the two pieces of returned data
        // which are, respectively, the offset at which the field occurs and the field's length
        uint8_t*  memo_ptr = SUB_OFFSET(memo_lookup) + memos;
        uint32_t  memo_len = SUB_LENGTH(memo_lookup);
 
        // memos are nested inside an actual memo object, so we need to subfield
        // equivalently in JSON this would look like memo_array[i]["Memo"]
        memo_lookup = sto_subfield(memo_ptr, memo_len, sfMemo);
        memo_ptr = SUB_OFFSET(memo_lookup) + memo_ptr;
        memo_len = SUB_LENGTH(memo_lookup);

        // now we lookup the subfields of the memo itself
        // again, equivalently this would look like memo_array[i]["Memo"]["MemoData"], ... etc.
        int64_t data_lookup = sto_subfield(memo_ptr, memo_len, sfMemoData);
        int64_t format_lookup = sto_subfield(memo_ptr, memo_len, sfMemoFormat);

        // if any of these lookups fail the request is malformed
        if (data_lookup < 0 || format_lookup < 0)
            rollback(SBUF("Memo transaction did not contain correct memo format."), 40);

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
    } // Memo reading


    // if memo payload is empty
    if (!(payload_ptr && signature_ptr && publickey_ptr))
        rollback(SBUF("Memo transaction did not contain XLS14 format."), 50);


    TRACESTR("Memo read: done");


    // checking memo payload
    int64_t lookup_flags    = sto_subfield(payload_ptr, payload_len, sfFlags);
    int64_t lookup_array    = sto_subfield(payload_ptr, payload_len, sfTemplate);


    if (lookup_flags < 0 || lookup_array < 0)
        rollback(SBUF("Validly signed memo lacked required STObject fields."), 70);
    
    TRACESTR("Memo payload checking: done");
    
    

    uint32_t flags = UINT32_FROM_BUF(SUB_OFFSET(lookup_flags) + payload_ptr);
    uint8_t* array_ptr = SUB_OFFSET(lookup_array) + payload_ptr;
    int array_len = SUB_LENGTH(lookup_array);


    // get hook account
    unsigned char hook_accid[20];
    hook_account((uint32_t)hook_accid, 20);


    // get sending account
    uint8_t account_field[20];
    int32_t account_field_len = otxn_field(SBUF(account_field), sfAccount);

    int equal = 0; 
    BUFFER_EQUAL(equal, hook_accid, account_field, 20);


    if (!equal)
    {
        // a buyer sending a tx with a memo of a Affiliate service


        // only one Affiliate account at time

        for (int i = 0; GUARD(1), i < 1; ++i) {

            int64_t lookup_array_entry = sto_subarray(array_ptr, array_len, i);

            if (lookup_array_entry < 0) break;

            uint8_t* array_entry_ptr = SUB_OFFSET(lookup_array_entry) + array_ptr;
            uint32_t array_entry_len = SUB_LENGTH(lookup_array_entry);

            trace(SBUF("Array entry: "), array_entry_ptr, array_entry_len, 0);   

            int64_t lookup_acc = sto_subfield(array_entry_ptr, array_entry_len, sfAccount);

            if (lookup_acc < 0)
                rollback(SBUF("Cachback: Invalid array entry, expecting sfAccount."), 80);


            uint8_t* acc_ptr = SUB_OFFSET(lookup_acc) + array_entry_ptr;
            uint32_t acc_len = SUB_LENGTH(lookup_acc);

            trace(SBUF("Account: "), acc_ptr, acc_len, 0);
            trace(SBUF("AccountHex: "), acc_ptr, acc_len, 1);   

            if (acc_len != 20)
                rollback(SBUF("Affiliate: Invalid sfAccount, expecting length = 20."), 90);

            

            uint8_t Affiliate_status[1] = { 0 };

            int64_t lookup = state(SBUF(Affiliate_status), acc_ptr, acc_len);

            if (lookup != 1 ) {
               accept(SBUF("The address specified in the memo is not set up as  affiliate. Regular transaction."), 0);
            }


            // fetch the sent Amount
            unsigned char amount_buffer[48];
            int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);


            if (amount_len != 8) {
                accept(SBUF("Non XRP token. Regular transaction."), 0);
            }


            int64_t otxn_drops = AMOUNT_TO_DROPS(amount_buffer);
            int64_t drops_to_send = (int64_t)((double)otxn_drops * 0.15f);
            int64_t drops_to_send_cb = (int64_t)((double)otxn_drops * 0.05f);


            // affiliate
            int64_t fee_base = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);
            unsigned char tx[PREPARE_PAYMENT_SIMPLE_SIZE];
            
            PREPARE_PAYMENT_SIMPLE(tx, drops_to_send++, fee_base, acc_ptr, 0, 0);
            uint8_t emithash[32];
            emit(SBUF(emithash), SBUF(tx));


            // cashback
            fee_base = etxn_fee_base(PREPARE_PAYMENT_SIMPLE_SIZE);
            unsigned char tx_cb[PREPARE_PAYMENT_SIMPLE_SIZE];

            PREPARE_PAYMENT_SIMPLE(tx_cb, drops_to_send_cb++, fee_base, account_field, 0, 0);
            uint8_t emithash_cb[32];
            emit(SBUF(emithash_cb), SBUF(tx_cb));

            accept(SBUF("Affiliate: Success"), 0);

        }
    }

    else {
        // hook owner specifying Affiliate accounts

        unsigned short processed_count = 0;

        for (int i = 0; GUARD(50), i < 50; ++i) {
            int64_t lookup_array_entry = sto_subarray(array_ptr, array_len, i);

            if (lookup_array_entry < 0) break;

            uint8_t* array_entry_ptr = SUB_OFFSET(lookup_array_entry) + array_ptr;
            uint32_t array_entry_len = SUB_LENGTH(lookup_array_entry);

            trace(SBUF("Array entry: "), array_entry_ptr, array_entry_len, 0);   

            int64_t lookup_acc = sto_subfield(array_entry_ptr, array_entry_len, sfAccount);

            if (lookup_acc < 0)
                rollback(SBUF("Affiliate: Invalid array entry, expecting sfAccount."), 80);


            uint8_t* acc_ptr = SUB_OFFSET(lookup_acc) + array_entry_ptr;
            uint32_t acc_len = SUB_LENGTH(lookup_acc);

            trace(SBUF("Account: "), acc_ptr, acc_len, 0);
            trace(SBUF("AccountHex: "), acc_ptr, acc_len, 1);   

            if (acc_len != 20)
                rollback(SBUF("Affiliate: Invalid sfAccount, expecting length = 20."), 90);

            uint8_t buffer[1] = {1}; // nominally we will simply a store a single byte = 1 for a blacklisted account
            uint32_t len = flags == 1 ? 1 : 0; // we will pass length = 0 to state_set for a delete operation
        

            if (state_set(buffer, len, acc_ptr, acc_len) == len)
                processed_count++;
            else
                trace(SBUF("Affiliate: Failed to update state for the following account."), acc_ptr, acc_len, 1);


        }

        RBUF(result_buffer, result_len, "Blacklist: Processed + ", processed_count);

        accept(SBUF("Affiliate: Set new Affiliate account(s)"), 0);

        
    }

    return 0;
}