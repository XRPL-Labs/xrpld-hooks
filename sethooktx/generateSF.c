
#include <stdio.h>

    // special types
int STI_UNKNOWN = -2;
int STI_DONE = -1;
int STI_NOTPRESENT = 0;

    // // types (common)
int STI_UINT16 = 1;
int STI_UINT32 = 2;
int STI_UINT64 = 3;
int STI_HASH128 = 4;
int STI_HASH256 = 5;
int STI_AMOUNT = 6;
int STI_VL = 7;
int STI_ACCOUNT = 8;
    // 9-13 are reserved
int STI_OBJECT = 14;
int STI_ARRAY = 15;

    // types (uncommon)
int STI_UINT8 = 16;
int STI_HASH160 = 17;
int STI_PATHSET = 18;
int STI_VECTOR256 = 19;

    // high level types
    // cannot be serialized inside other types
int STI_TRANSACTION = 10001;
int STI_LEDGERENTRY = 10002;
int STI_VALIDATION = 10003;
int STI_METADATA = 10004;


int field_code(int type, int index, void* dummy) {
    return (type << 16) | index;
}

int main() {



printf("#define sfInvalid -1\n");
printf("#define sfGeneric 0\n");
printf("#define sfLedgerEntry 0x%04x\n", field_code( STI_LEDGERENTRY, 257, "LedgerEntry"));
printf("#define sfTransaction 0x%04x\n", field_code( STI_TRANSACTION, 257, "Transaction"));
printf("#define sfValidation 0x%04x\n", field_code( STI_VALIDATION, 257, "Validation"));
printf("#define sfMetadata 0x%04x\n", field_code( STI_METADATA, 257, "Metadata"));
printf("#define sfHash 0x%04x\n", field_code( STI_HASH256, 257, "hash"));
printf("#define sfIndex 0x%04x\n", field_code( STI_HASH256, 258, "index"));

// 8-bit integers
printf("#define sfCloseResolution 0x%04x\n", field_code( STI_UINT8, 1, "CloseResolution"));
printf("#define sfMethod 0x%04x\n", field_code( STI_UINT8, 2, "Method"));
printf("#define sfTransactionResult 0x%04x\n", field_code( STI_UINT8, 3, "TransactionResult"));

// 8-bit integers (uncommon)
printf("#define sfTickSize 0x%04x\n", field_code( STI_UINT8, 16, "TickSize"));
printf("#define sfUNLModifyDisabling 0x%04x\n", field_code( STI_UINT8, 17, "UNLModifyDisabling"));

// 16-bit integers
printf("#define sfLedgerEntryType 0x%04x\n", field_code( STI_UINT16, 1, "LedgerEntryType" ) );
printf("#define sfTransactionType 0x%04x\n", field_code( STI_UINT16, 2, "TransactionType"));
printf("#define sfSignerWeight 0x%04x\n", field_code( STI_UINT16, 3, "SignerWeight"));

// 16-bit integers (uncommon)
printf("#define sfVersion 0x%04x\n", field_code( STI_UINT16, 16, "Version"));

// 32-bit integers (common)
printf("#define sfFlags 0x%04x\n", field_code( STI_UINT32, 2, "Flags"));
printf("#define sfSourceTag 0x%04x\n", field_code( STI_UINT32, 3, "SourceTag"));
printf("#define sfSequence 0x%04x\n", field_code( STI_UINT32, 4, "Sequence"));
printf("#define sfPreviousTxnLgrSeq 0x%04x\n", field_code(STI_UINT32,5,"PreviousTxnLgrSeq"));

printf("#define sfLedgerSequence 0x%04x\n", field_code( STI_UINT32, 6, "LedgerSequence"));
printf("#define sfCloseTime 0x%04x\n", field_code( STI_UINT32, 7, "CloseTime"));
printf("#define sfParentCloseTime 0x%04x\n", field_code( STI_UINT32, 8, "ParentCloseTime"));
printf("#define sfSigningTime 0x%04x\n", field_code( STI_UINT32, 9, "SigningTime"));
printf("#define sfExpiration 0x%04x\n", field_code( STI_UINT32, 10, "Expiration"));
printf("#define sfTransferRate 0x%04x\n", field_code( STI_UINT32, 11, "TransferRate"));
printf("#define sfWalletSize 0x%04x\n", field_code( STI_UINT32, 12, "WalletSize"));
printf("#define sfOwnerCount 0x%04x\n", field_code( STI_UINT32, 13, "OwnerCount"));
printf("#define sfDestinationTag 0x%04x\n", field_code( STI_UINT32, 14, "DestinationTag"));

// 32-bit integers (uncommon)
printf("#define sfHighQualityIn 0x%04x\n", field_code( STI_UINT32, 16, "HighQualityIn"));
printf("#define sfHighQualityOut 0x%04x\n", field_code( STI_UINT32, 17, "HighQualityOut"));
printf("#define sfLowQualityIn 0x%04x\n", field_code( STI_UINT32, 18, "LowQualityIn"));
printf("#define sfLowQualityOut 0x%04x\n", field_code( STI_UINT32, 19, "LowQualityOut"));
printf("#define sfQualityIn 0x%04x\n", field_code( STI_UINT32, 20, "QualityIn"));
printf("#define sfQualityOut 0x%04x\n", field_code( STI_UINT32, 21, "QualityOut"));
printf("#define sfStampEscrow 0x%04x\n", field_code( STI_UINT32, 22, "StampEscrow"));
printf("#define sfBondAmount 0x%04x\n", field_code( STI_UINT32, 23, "BondAmount"));
printf("#define sfLoadFee 0x%04x\n", field_code( STI_UINT32, 24, "LoadFee"));
printf("#define sfOfferSequence 0x%04x\n", field_code( STI_UINT32, 25, "OfferSequence"));
printf("#define sfFirstLedgerSequence 0x%04x\n", field_code( STI_UINT32, 26, "FirstLedgerSequence"));
printf("#define sfLastLedgerSequence 0x%04x\n", field_code( STI_UINT32, 27, "LastLedgerSequence"));
printf("#define sfTransactionIndex 0x%04x\n", field_code( STI_UINT32, 28, "TransactionIndex"));
printf("#define sfOperationLimit 0x%04x\n", field_code( STI_UINT32, 29, "OperationLimit"));
printf("#define sfReferenceFeeUnits 0x%04x\n", field_code( STI_UINT32, 30, "ReferenceFeeUnits"));
printf("#define sfReserveBase 0x%04x\n", field_code( STI_UINT32, 31, "ReserveBase"));
printf("#define sfReserveIncrement 0x%04x\n", field_code( STI_UINT32, 32, "ReserveIncrement"));
printf("#define sfSetFlag 0x%04x\n", field_code( STI_UINT32, 33, "SetFlag"));
printf("#define sfClearFlag 0x%04x\n", field_code( STI_UINT32, 34, "ClearFlag"));
printf("#define sfSignerQuorum 0x%04x\n", field_code( STI_UINT32, 35, "SignerQuorum"));
printf("#define sfCancelAfter 0x%04x\n", field_code( STI_UINT32, 36, "CancelAfter"));
printf("#define sfFinishAfter 0x%04x\n", field_code( STI_UINT32, 37, "FinishAfter"));
printf("#define sfSignerListID 0x%04x\n", field_code( STI_UINT32, 38, "SignerListID"));
printf("#define sfSettleDelay 0x%04x\n", field_code( STI_UINT32, 39, "SettleDelay"));
printf("#define sfHookStateCount 0x%04x\n", field_code( STI_UINT32, 40, "HookStateCount"));
printf("#define sfHookReserveCount 0x%04x\n", field_code( STI_UINT32, 41, "HookReserveCount"));
printf("#define sfHookDataMaxSize 0x%04x\n", field_code( STI_UINT32, 42, "HookDataMaxSize"));
printf("#define sfEmitGeneration 0x%04x\n", field_code( STI_UINT32, 43, "EmitGeneration"));

// 64-bit integers
printf("#define sfIndexNext 0x%04x\n", field_code( STI_UINT64, 1, "IndexNext"));
printf("#define sfIndexPrevious 0x%04x\n", field_code( STI_UINT64, 2, "IndexPrevious"));
printf("#define sfBookNode 0x%04x\n", field_code( STI_UINT64, 3, "BookNode"));
printf("#define sfOwnerNode 0x%04x\n", field_code( STI_UINT64, 4, "OwnerNode"));
printf("#define sfBaseFee 0x%04x\n", field_code( STI_UINT64, 5, "BaseFee"));
printf("#define sfExchangeRate 0x%04x\n", field_code( STI_UINT64, 6, "ExchangeRate"));
printf("#define sfLowNode 0x%04x\n", field_code( STI_UINT64, 7, "LowNode"));
printf("#define sfHighNode 0x%04x\n", field_code( STI_UINT64, 8, "HighNode"));
printf("#define sfDestinationNode 0x%04x\n", field_code( STI_UINT64, 9, "DestinationNode"));
printf("#define sfCookie 0x%04x\n", field_code( STI_UINT64, 10, "Cookie"));
printf("#define sfServerVersion 0x%04x\n", field_code( STI_UINT64, 11, "ServerVersion"));
printf("#define sfEmitBurden 0x%04x\n", field_code( STI_UINT64, 12, "EmitBurden"));

// 64bit uncommon
printf("#define sfHookOn 0x%04x\n", field_code( STI_UINT64, 16, "HookOn"));

// 128-bit
printf("#define sfEmailHash 0x%04x\n", field_code( STI_HASH128, 1, "EmailHash"));

// 160-bit (common)
printf("#define sfTakerPaysCurrency 0x%04x\n", field_code( STI_HASH160, 1, "TakerPaysCurrency"));
printf("#define sfTakerPaysIssuer 0x%04x\n", field_code( STI_HASH160, 2, "TakerPaysIssuer"));
printf("#define sfTakerGetsCurrency 0x%04x\n", field_code( STI_HASH160, 3, "TakerGetsCurrency"));
printf("#define sfTakerGetsIssuer 0x%04x\n", field_code( STI_HASH160, 4, "TakerGetsIssuer"));

// 256-bit (common)
printf("#define sfLedgerHash 0x%04x\n", field_code( STI_HASH256, 1, "LedgerHash"));
printf("#define sfParentHash 0x%04x\n", field_code( STI_HASH256, 2, "ParentHash"));
printf("#define sfTransactionHash 0x%04x\n", field_code( STI_HASH256, 3, "TransactionHash"));
printf("#define sfAccountHash 0x%04x\n", field_code( STI_HASH256, 4, "AccountHash"));
printf("#define sfPreviousTxnID 0x%04x\n", field_code( STI_HASH256, 5, "PreviousTxnID" ));
printf("#define sfLedgerIndex 0x%04x\n", field_code( STI_HASH256, 6, "LedgerIndex"));
printf("#define sfWalletLocator 0x%04x\n", field_code( STI_HASH256, 7, "WalletLocator"));
printf("#define sfRootIndex 0x%04x\n", field_code( STI_HASH256, 8, "RootIndex"));
printf("#define sfAccountTxnID 0x%04x\n", field_code( STI_HASH256, 9, "AccountTxnID"));
printf("#define sfEmitParentTxnID 0x%04x\n", field_code( STI_HASH256, 10, "EmitParentTxnID"));
printf("#define sfEmitNonce 0x%04x\n", field_code( STI_HASH256, 11, "EmitNonce"));

// 256-bit (uncommon)
printf("#define sfBookDirectory 0x%04x\n", field_code( STI_HASH256, 16, "BookDirectory"));
printf("#define sfInvoiceID 0x%04x\n", field_code( STI_HASH256, 17, "InvoiceID"));
printf("#define sfNickname 0x%04x\n", field_code( STI_HASH256, 18, "Nickname"));
printf("#define sfAmendment 0x%04x\n", field_code( STI_HASH256, 19, "Amendment"));
printf("#define sfTicketID 0x%04x\n", field_code( STI_HASH256, 20, "TicketID"));
printf("#define sfDigest 0x%04x\n", field_code( STI_HASH256, 21, "Digest"));
printf("#define sfPayChannel 0x%04x\n", field_code( STI_HASH256, 22, "Channel"));
printf("#define sfConsensusHash 0x%04x\n", field_code( STI_HASH256, 23, "ConsensusHash"));
printf("#define sfCheckID 0x%04x\n", field_code( STI_HASH256, 24, "CheckID"));
printf("#define sfValidatedHash 0x%04x\n", field_code( STI_HASH256, 25, "ValidatedHash"));

// currency amount (common)
printf("#define sfAmount 0x%04x\n", field_code( STI_AMOUNT, 1, "Amount"));
printf("#define sfBalance 0x%04x\n", field_code( STI_AMOUNT, 2, "Balance"));
printf("#define sfLimitAmount 0x%04x\n", field_code( STI_AMOUNT, 3, "LimitAmount"));
printf("#define sfTakerPays 0x%04x\n", field_code( STI_AMOUNT, 4, "TakerPays"));
printf("#define sfTakerGets 0x%04x\n", field_code( STI_AMOUNT, 5, "TakerGets"));
printf("#define sfLowLimit 0x%04x\n", field_code( STI_AMOUNT, 6, "LowLimit"));
printf("#define sfHighLimit 0x%04x\n", field_code( STI_AMOUNT, 7, "HighLimit"));
printf("#define sfFee 0x%04x\n", field_code( STI_AMOUNT, 8, "Fee"));
printf("#define sfSendMax 0x%04x\n", field_code( STI_AMOUNT, 9, "SendMax"));
printf("#define sfDeliverMin 0x%04x\n", field_code( STI_AMOUNT, 10, "DeliverMin"));

// currency amount (uncommon)
printf("#define sfMinimumOffer 0x%04x\n", field_code( STI_AMOUNT, 16, "MinimumOffer"));
printf("#define sfRippleEscrow 0x%04x\n", field_code( STI_AMOUNT, 17, "RippleEscrow"));
printf("#define sfDeliveredAmount 0x%04x\n", field_code( STI_AMOUNT, 18, "DeliveredAmount"));

// variable length (common)
printf("#define sfPublicKey 0x%04x\n", field_code( STI_VL, 1, "PublicKey"));
printf("#define sfMessageKey 0x%04x\n", field_code( STI_VL, 2, "MessageKey"));
printf("#define sfSigningPubKey 0x%04x\n", field_code( STI_VL, 3, "SigningPubKey"));
printf("#define sfTxnSignature 0x%04x\n", field_code(STI_VL, 4, "TxnSignature" ));
printf("#define sfSignature 0x%04x\n", field_code(STI_VL, 6, "Signature" ));
printf("#define sfDomain 0x%04x\n", field_code( STI_VL, 7, "Domain"));
printf("#define sfFundCode 0x%04x\n", field_code( STI_VL, 8, "FundCode"));
printf("#define sfRemoveCode 0x%04x\n", field_code( STI_VL, 9, "RemoveCode"));
printf("#define sfExpireCode 0x%04x\n", field_code( STI_VL, 10, "ExpireCode"));
printf("#define sfCreateCode 0x%04x\n", field_code( STI_VL, 11, "CreateCode"));
printf("#define sfMemoType 0x%04x\n", field_code( STI_VL, 12, "MemoType"));
printf("#define sfMemoData 0x%04x\n", field_code( STI_VL, 13, "MemoData"));
printf("#define sfMemoFormat 0x%04x\n", field_code( STI_VL, 14, "MemoFormat"));

// variable length (uncommon)
printf("#define sfFulfillment 0x%04x\n", field_code( STI_VL, 16, "Fulfillment"));
printf("#define sfCondition 0x%04x\n", field_code( STI_VL, 17, "Condition"));
printf("#define sfMasterSignature 0x%04x\n", field_code(STI_VL, 18, "MasterSignature"));
printf("#define sfUNLModifyValidator 0x%04x\n", field_code( STI_VL, 19, "UNLModifyValidator"));
printf("#define sfNegativeUNLToDisable 0x%04x\n", field_code( STI_VL, 20, "ValidatorToDisable"));
printf("#define sfNegativeUNLToReEnable 0x%04x\n", field_code( STI_VL, 21, "ValidatorToReEnable"));
printf("#define sfHookData 0x%04x\n", field_code( STI_VL, 22, "HookData"));

// account
printf("#define sfAccount 0x%04x\n", field_code( STI_ACCOUNT, 1, "Account"));
printf("#define sfOwner 0x%04x\n", field_code( STI_ACCOUNT, 2, "Owner"));
printf("#define sfDestination 0x%04x\n", field_code( STI_ACCOUNT, 3, "Destination"));
printf("#define sfIssuer 0x%04x\n", field_code( STI_ACCOUNT, 4, "Issuer"));
printf("#define sfAuthorize 0x%04x\n", field_code( STI_ACCOUNT, 5, "Authorize"));
printf("#define sfUnauthorize 0x%04x\n", field_code( STI_ACCOUNT, 6, "Unauthorize"));
printf("#define sfTarget 0x%04x\n", field_code( STI_ACCOUNT, 7, "Target"));
printf("#define sfRegularKey 0x%04x\n", field_code( STI_ACCOUNT, 8, "RegularKey"));

// path set
printf("#define sfPaths 0x%04x\n", field_code( STI_PATHSET, 1, "Paths"));

// vector of 256-bit
printf("#define sfIndexes 0x%04x\n", field_code( STI_VECTOR256, 1, "Indexes"));
printf("#define sfHashes 0x%04x\n", field_code( STI_VECTOR256, 2, "Hashes"));
printf("#define sfAmendments 0x%04x\n", field_code( STI_VECTOR256, 3, "Amendments"));

// inner object
// OBJECT/1 is reserved for end of object
printf("#define sfTransactionMetaData 0x%04x\n", field_code( STI_OBJECT, 2, "TransactionMetaData"));
printf("#define sfCreatedNode 0x%04x\n", field_code( STI_OBJECT, 3, "CreatedNode"));
printf("#define sfDeletedNode 0x%04x\n", field_code( STI_OBJECT, 4, "DeletedNode"));
printf("#define sfModifiedNode 0x%04x\n", field_code( STI_OBJECT, 5, "ModifiedNode"));
printf("#define sfPreviousFields 0x%04x\n", field_code( STI_OBJECT, 6, "PreviousFields"));
printf("#define sfFinalFields 0x%04x\n", field_code( STI_OBJECT, 7, "FinalFields"));
printf("#define sfNewFields 0x%04x\n", field_code( STI_OBJECT, 8, "NewFields"));
printf("#define sfTemplateEntry 0x%04x\n", field_code( STI_OBJECT, 9, "TemplateEntry"));
printf("#define sfMemo 0x%04x\n", field_code( STI_OBJECT, 10, "Memo"));
printf("#define sfSignerEntry 0x%04x\n", field_code( STI_OBJECT, 11, "SignerEntry"));
printf("#define sfEmitDetails 0x%04x\n", field_code( STI_OBJECT, 12, "EmitDetails"));

// inner object (uncommon)
printf("#define sfSigner 0x%04x\n", field_code( STI_OBJECT, 16, "Signer"));
//                                                                                 17 has not been used yet...
printf("#define sfMajority 0x%04x\n", field_code( STI_OBJECT, 18, "Majority"));
printf("#define sfNegativeUNLEntry 0x%04x\n", field_code( STI_OBJECT, 19, "DisabledValidator"));
printf("#define sfEmitDetails 0x%04x\n", field_code( STI_OBJECT, 20, "EmitDetails"));

// array of objects
// ARRAY/1 is reserved for end of array
printf("#define sfSigningAccounts  0x%04x\n", field_code( STI_ARRAY, 2, "SigningAccounts")), //
// Never been used.
printf("#define sfSigners 0x%04x\n", field_code(STI_ARRAY, 3, "Signers"));
printf("#define sfSignerEntries 0x%04x\n", field_code( STI_ARRAY, 4, "SignerEntries"));
printf("#define sfTemplate 0x%04x\n", field_code( STI_ARRAY, 5, "Template"));
printf("#define sfNecessary 0x%04x\n", field_code( STI_ARRAY, 6, "Necessary"));
printf("#define sfSufficient 0x%04x\n", field_code( STI_ARRAY, 7, "Sufficient"));
printf("#define sfAffectedNodes 0x%04x\n", field_code( STI_ARRAY, 8, "AffectedNodes"));
printf("#define sfMemos 0x%04x\n", field_code( STI_ARRAY, 9, "Memos"));

// array of objects (uncommon)
printf("#define sfMajorities 0x%04x\n", field_code( STI_ARRAY, 16, "Majorities"));
printf("#define sfNegativeUNL 0x%04x\n", field_code( STI_ARRAY, 17, "NegativeUNL"));

return 0;
}
