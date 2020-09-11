
extern int64_t output_dbg   ( unsigned char* buf, int32_t len );
extern int64_t set_state    ( unsigned char* key_ptr, unsigned char* data_ptr_in, uint32_t in_len );
extern int64_t get_state    ( unsigned char* key_ptr, unsigned char* data_ptr_out, uint32_t out_len );
extern int64_t accept       ( int32_t error_code, unsigned char* data_ptr_in, uint32_t in_len );
extern int64_t reject       ( int32_t error_code, unsigned char* data_ptr_in, uint32_t in_len );
extern int64_t rollback     ( int32_t error_code, unsigned char* data_ptr_in, uint32_t in_len );
extern int64_t get_tx_type  ( );
extern int64_t get_tx_field ( uint32_t field_id, uint32_t data_ptr_out, uint32_t out_len );

#define BUF_TO_DEC(buf, len, numout)\
    {\
        int i = 0;\
        numout = 0;\
        for (; i < len && buf[i] != 0; ++i) {\
            if (buf[i] < '0' || buf[i] > '9') {\
                numout = 0;\
                break;\
            }\
            numout *= 10;\
            numout += (buf[i]-'0');\
        }\
    }

#define DEC_TO_BUF(numin, buf, len)\
    {\
        int digit_count = 0;\
        int numin2 = numin;\
        int numin3 = numin;\
        for (; numin2 > 0; ++digit_count) numin2 /= 10;\
        if (digit_count < len - 1) {\
            buf[digit_count] = 0;\
            for (; digit_count > 0; --digit_count, numin3 /= 10)\
                buf[digit_count] = '0' + (numin3 % 10);\
        }\
    }

#define STRLEN(buf, maxlen, lenout)\
    {\
        lenout = 0;\
        for (; buf[lenout] != 0 && lenout < maxlen; ++lenout);\
    }


#define sfInvalid -1
#define sfGeneric 0
#define sfLedgerEntry 655491329
#define sfTransaction 655425793
#define sfValidation 655556865
#define sfMetadata 655622401
#define sfHash 327937
#define sfIndex 327938
#define sfCloseResolution 1048577
#define sfMethod 1048578
#define sfTransactionResult 1048579
#define sfTickSize 1048592
#define sfUNLModifyDisabling 1048593
#define sfLedgerEntryType 65537
#define sfTransactionType 65538
#define sfSignerWeight 65539
#define sfVersion 65552
#define sfFlags 131074
#define sfSourceTag 131075
#define sfSequence 131076
#define sfPreviousTxnLgrSeq 131077
#define sfLedgerSequence 131078
#define sfCloseTime 131079
#define sfParentCloseTime 131080
#define sfSigningTime 131081
#define sfExpiration 131082
#define sfTransferRate 131083
#define sfWalletSize 131084
#define sfOwnerCount 131085
#define sfDestinationTag 131086
#define sfHighQualityIn 131088
#define sfHighQualityOut 131089
#define sfLowQualityIn 131090
#define sfLowQualityOut 131091
#define sfQualityIn 131092
#define sfQualityOut 131093
#define sfStampEscrow 131094
#define sfBondAmount 131095
#define sfLoadFee 131096
#define sfOfferSequence 131097
#define sfFirstLedgerSequence 131098
#define sfLastLedgerSequence 131099
#define sfTransactionIndex 131100
#define sfOperationLimit 131101
#define sfReferenceFeeUnits 131102
#define sfReserveBase 131103
#define sfReserveIncrement 131104
#define sfSetFlag 131105
#define sfClearFlag 131106
#define sfSignerQuorum 131107
#define sfCancelAfter 131108
#define sfFinishAfter 131109
#define sfSignerListID 131110
#define sfSettleDelay 131111
#define sfHookStateCount 131112
#define sfHookReserveCount 131113
#define sfHookDataMaxSize 131114
#define sfIndexNext 196609
#define sfIndexPrevious 196610
#define sfBookNode 196611
#define sfOwnerNode 196612
#define sfBaseFee 196613
#define sfExchangeRate 196614
#define sfLowNode 196615
#define sfHighNode 196616
#define sfDestinationNode 196617
#define sfCookie 196618
#define sfServerVersion 196619
#define sfHookOn 196620
#define sfEmailHash 262145
#define sfTakerPaysCurrency 1114113
#define sfTakerPaysIssuer 1114114
#define sfTakerGetsCurrency 1114115
#define sfTakerGetsIssuer 1114116
#define sfLedgerHash 327681
#define sfParentHash 327682
#define sfTransactionHash 327683
#define sfAccountHash 327684
#define sfPreviousTxnID 327685
#define sfLedgerIndex 327686
#define sfWalletLocator 327687
#define sfRootIndex 327688
#define sfAccountTxnID 327689
#define sfBookDirectory 327696
#define sfInvoiceID 327697
#define sfNickname 327698
#define sfAmendment 327699
#define sfTicketID 327700
#define sfDigest 327701
#define sfPayChannel 327702
#define sfConsensusHash 327703
#define sfCheckID 327704
#define sfValidatedHash 327705
#define sfAmount 393217
#define sfBalance 393218
#define sfLimitAmount 393219
#define sfTakerPays 393220
#define sfTakerGets 393221
#define sfLowLimit 393222
#define sfHighLimit 393223
#define sfFee 393224
#define sfSendMax 393225
#define sfDeliverMin 393226
#define sfMinimumOffer 393232
#define sfRippleEscrow 393233
#define sfDeliveredAmount 393234
#define sfPublicKey 458753
#define sfMessageKey 458754
#define sfSigningPubKey 458755
#define sfTxnSignature 458756
#define sfSignature 458758
#define sfDomain 458759
#define sfFundCode 458760
#define sfRemoveCode 458761
#define sfExpireCode 458762
#define sfCreateCode 458763
#define sfMemoType 458764
#define sfMemoData 458765
#define sfMemoFormat 458766
#define sfFulfillment 458768
#define sfCondition 458769
#define sfMasterSignature 458770
#define sfUNLModifyValidator 458771
#define sfNegativeUNLToDisable 458772
#define sfNegativeUNLToReEnable 458773
#define sfHookData 458774
#define sfAccount 524289
#define sfOwner 524290
#define sfDestination 524291
#define sfIssuer 524292
#define sfAuthorize 524293
#define sfUnauthorize 524294
#define sfTarget 524295
#define sfRegularKey 524296
#define sfPaths 1179649
#define sfIndexes 1245185
#define sfHashes 1245186
#define sfAmendments 1245187
#define sfTransactionMetaData 917506
#define sfCreatedNode 917507
#define sfDeletedNode 917508
#define sfModifiedNode 917509
#define sfPreviousFields 917510
#define sfFinalFields 917511
#define sfNewFields 917512
#define sfTemplateEntry 917513
#define sfMemo 917514
#define sfSignerEntry 917515
#define sfSigner 917520
#define sfMajority 917522
#define sfNegativeUNLEntry 917523
#define sfSigningAccounts  983042
#define sfSigners 983043
#define sfSignerEntries 983044
#define sfTemplate 983045
#define sfNecessary 983046
#define sfSufficient 983047
#define sfAffectedNodes 983048
#define sfMemos 983049
#define sfMajorities 983056
#define sfNegativeUNL 983057



