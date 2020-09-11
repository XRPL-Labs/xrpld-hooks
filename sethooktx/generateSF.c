
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
printf("#define sfLedgerEntry %d\n", field_code( STI_LEDGERENTRY, 257, "LedgerEntry"));
printf("#define sfTransaction %d\n", field_code( STI_TRANSACTION, 257, "Transaction"));
printf("#define sfValidation %d\n", field_code( STI_VALIDATION, 257, "Validation"));
printf("#define sfMetadata %d\n", field_code( STI_METADATA, 257, "Metadata"));
printf("#define sfHash %d\n", field_code( STI_HASH256, 257, "hash"));
printf("#define sfIndex %d\n", field_code( STI_HASH256, 258, "index"));

// 8-bit integers
printf("#define sfCloseResolution %d\n", field_code( STI_UINT8, 1, "CloseResolution"));
printf("#define sfMethod %d\n", field_code( STI_UINT8, 2, "Method"));
printf("#define sfTransactionResult %d\n", field_code( STI_UINT8, 3, "TransactionResult"));

// 8-bit integers (uncommon)
printf("#define sfTickSize %d\n", field_code( STI_UINT8, 16, "TickSize"));
printf("#define sfUNLModifyDisabling %d\n", field_code( STI_UINT8, 17, "UNLModifyDisabling"));

// 16-bit integers
printf("#define sfLedgerEntryType %d\n", field_code( STI_UINT16, 1, "LedgerEntryType" ) );
printf("#define sfTransactionType %d\n", field_code( STI_UINT16, 2, "TransactionType"));
printf("#define sfSignerWeight %d\n", field_code( STI_UINT16, 3, "SignerWeight"));

// 16-bit integers (uncommon)
printf("#define sfVersion %d\n", field_code( STI_UINT16, 16, "Version"));

// 32-bit integers (common)
printf("#define sfFlags %d\n", field_code( STI_UINT32, 2, "Flags"));
printf("#define sfSourceTag %d\n", field_code( STI_UINT32, 3, "SourceTag"));
printf("#define sfSequence %d\n", field_code( STI_UINT32, 4, "Sequence"));
printf("#define sfPreviousTxnLgrSeq %d\n", field_code(STI_UINT32,5,"PreviousTxnLgrSeq"));

printf("#define sfLedgerSequence %d\n", field_code( STI_UINT32, 6, "LedgerSequence"));
printf("#define sfCloseTime %d\n", field_code( STI_UINT32, 7, "CloseTime"));
printf("#define sfParentCloseTime %d\n", field_code( STI_UINT32, 8, "ParentCloseTime"));
printf("#define sfSigningTime %d\n", field_code( STI_UINT32, 9, "SigningTime"));
printf("#define sfExpiration %d\n", field_code( STI_UINT32, 10, "Expiration"));
printf("#define sfTransferRate %d\n", field_code( STI_UINT32, 11, "TransferRate"));
printf("#define sfWalletSize %d\n", field_code( STI_UINT32, 12, "WalletSize"));
printf("#define sfOwnerCount %d\n", field_code( STI_UINT32, 13, "OwnerCount"));
printf("#define sfDestinationTag %d\n", field_code( STI_UINT32, 14, "DestinationTag"));

// 32-bit integers (uncommon)
printf("#define sfHighQualityIn %d\n", field_code( STI_UINT32, 16, "HighQualityIn"));
printf("#define sfHighQualityOut %d\n", field_code( STI_UINT32, 17, "HighQualityOut"));
printf("#define sfLowQualityIn %d\n", field_code( STI_UINT32, 18, "LowQualityIn"));
printf("#define sfLowQualityOut %d\n", field_code( STI_UINT32, 19, "LowQualityOut"));
printf("#define sfQualityIn %d\n", field_code( STI_UINT32, 20, "QualityIn"));
printf("#define sfQualityOut %d\n", field_code( STI_UINT32, 21, "QualityOut"));
printf("#define sfStampEscrow %d\n", field_code( STI_UINT32, 22, "StampEscrow"));
printf("#define sfBondAmount %d\n", field_code( STI_UINT32, 23, "BondAmount"));
printf("#define sfLoadFee %d\n", field_code( STI_UINT32, 24, "LoadFee"));
printf("#define sfOfferSequence %d\n", field_code( STI_UINT32, 25, "OfferSequence"));
printf("#define sfFirstLedgerSequence %d\n", field_code( STI_UINT32, 26, "FirstLedgerSequence"));
printf("#define sfLastLedgerSequence %d\n", field_code( STI_UINT32, 27, "LastLedgerSequence"));
printf("#define sfTransactionIndex %d\n", field_code( STI_UINT32, 28, "TransactionIndex"));
printf("#define sfOperationLimit %d\n", field_code( STI_UINT32, 29, "OperationLimit"));
printf("#define sfReferenceFeeUnits %d\n", field_code( STI_UINT32, 30, "ReferenceFeeUnits"));
printf("#define sfReserveBase %d\n", field_code( STI_UINT32, 31, "ReserveBase"));
printf("#define sfReserveIncrement %d\n", field_code( STI_UINT32, 32, "ReserveIncrement"));
printf("#define sfSetFlag %d\n", field_code( STI_UINT32, 33, "SetFlag"));
printf("#define sfClearFlag %d\n", field_code( STI_UINT32, 34, "ClearFlag"));
printf("#define sfSignerQuorum %d\n", field_code( STI_UINT32, 35, "SignerQuorum"));
printf("#define sfCancelAfter %d\n", field_code( STI_UINT32, 36, "CancelAfter"));
printf("#define sfFinishAfter %d\n", field_code( STI_UINT32, 37, "FinishAfter"));
printf("#define sfSignerListID %d\n", field_code( STI_UINT32, 38, "SignerListID"));
printf("#define sfSettleDelay %d\n", field_code( STI_UINT32, 39, "SettleDelay"));
printf("#define sfHookStateCount %d\n", field_code( STI_UINT32, 40, "HookStateCount"));
printf("#define sfHookReserveCount %d\n", field_code( STI_UINT32, 41, "HookReserveCount"));
printf("#define sfHookDataMaxSize %d\n", field_code( STI_UINT32, 42, "HookDataMaxSize"));

// 64-bit integers
printf("#define sfIndexNext %d\n", field_code( STI_UINT64, 1, "IndexNext"));
printf("#define sfIndexPrevious %d\n", field_code( STI_UINT64, 2, "IndexPrevious"));
printf("#define sfBookNode %d\n", field_code( STI_UINT64, 3, "BookNode"));
printf("#define sfOwnerNode %d\n", field_code( STI_UINT64, 4, "OwnerNode"));
printf("#define sfBaseFee %d\n", field_code( STI_UINT64, 5, "BaseFee"));
printf("#define sfExchangeRate %d\n", field_code( STI_UINT64, 6, "ExchangeRate"));
printf("#define sfLowNode %d\n", field_code( STI_UINT64, 7, "LowNode"));
printf("#define sfHighNode %d\n", field_code( STI_UINT64, 8, "HighNode"));
printf("#define sfDestinationNode %d\n", field_code( STI_UINT64, 9, "DestinationNode"));
printf("#define sfCookie %d\n", field_code( STI_UINT64, 10, "Cookie"));
printf("#define sfServerVersion %d\n", field_code( STI_UINT64, 11, "ServerVersion"));
printf("#define sfHookOn %d\n", field_code( STI_UINT64, 12, "HookOn"));

// 128-bit
printf("#define sfEmailHash %d\n", field_code( STI_HASH128, 1, "EmailHash"));

// 160-bit (common)
printf("#define sfTakerPaysCurrency %d\n", field_code( STI_HASH160, 1, "TakerPaysCurrency"));
printf("#define sfTakerPaysIssuer %d\n", field_code( STI_HASH160, 2, "TakerPaysIssuer"));
printf("#define sfTakerGetsCurrency %d\n", field_code( STI_HASH160, 3, "TakerGetsCurrency"));
printf("#define sfTakerGetsIssuer %d\n", field_code( STI_HASH160, 4, "TakerGetsIssuer"));

// 256-bit (common)
printf("#define sfLedgerHash %d\n", field_code( STI_HASH256, 1, "LedgerHash"));
printf("#define sfParentHash %d\n", field_code( STI_HASH256, 2, "ParentHash"));
printf("#define sfTransactionHash %d\n", field_code( STI_HASH256, 3, "TransactionHash"));
printf("#define sfAccountHash %d\n", field_code( STI_HASH256, 4, "AccountHash"));
printf("#define sfPreviousTxnID %d\n", field_code( STI_HASH256, 5, "PreviousTxnID" ));
printf("#define sfLedgerIndex %d\n", field_code( STI_HASH256, 6, "LedgerIndex"));
printf("#define sfWalletLocator %d\n", field_code( STI_HASH256, 7, "WalletLocator"));
printf("#define sfRootIndex %d\n", field_code( STI_HASH256, 8, "RootIndex"));
printf("#define sfAccountTxnID %d\n", field_code( STI_HASH256, 9, "AccountTxnID"));

// 256-bit (uncommon)
printf("#define sfBookDirectory %d\n", field_code( STI_HASH256, 16, "BookDirectory"));
printf("#define sfInvoiceID %d\n", field_code( STI_HASH256, 17, "InvoiceID"));
printf("#define sfNickname %d\n", field_code( STI_HASH256, 18, "Nickname"));
printf("#define sfAmendment %d\n", field_code( STI_HASH256, 19, "Amendment"));
printf("#define sfTicketID %d\n", field_code( STI_HASH256, 20, "TicketID"));
printf("#define sfDigest %d\n", field_code( STI_HASH256, 21, "Digest"));
printf("#define sfPayChannel %d\n", field_code( STI_HASH256, 22, "Channel"));
printf("#define sfConsensusHash %d\n", field_code( STI_HASH256, 23, "ConsensusHash"));
printf("#define sfCheckID %d\n", field_code( STI_HASH256, 24, "CheckID"));
printf("#define sfValidatedHash %d\n", field_code( STI_HASH256, 25, "ValidatedHash"));

// currency amount (common)
printf("#define sfAmount %d\n", field_code( STI_AMOUNT, 1, "Amount"));
printf("#define sfBalance %d\n", field_code( STI_AMOUNT, 2, "Balance"));
printf("#define sfLimitAmount %d\n", field_code( STI_AMOUNT, 3, "LimitAmount"));
printf("#define sfTakerPays %d\n", field_code( STI_AMOUNT, 4, "TakerPays"));
printf("#define sfTakerGets %d\n", field_code( STI_AMOUNT, 5, "TakerGets"));
printf("#define sfLowLimit %d\n", field_code( STI_AMOUNT, 6, "LowLimit"));
printf("#define sfHighLimit %d\n", field_code( STI_AMOUNT, 7, "HighLimit"));
printf("#define sfFee %d\n", field_code( STI_AMOUNT, 8, "Fee"));
printf("#define sfSendMax %d\n", field_code( STI_AMOUNT, 9, "SendMax"));
printf("#define sfDeliverMin %d\n", field_code( STI_AMOUNT, 10, "DeliverMin"));

// currency amount (uncommon)
printf("#define sfMinimumOffer %d\n", field_code( STI_AMOUNT, 16, "MinimumOffer"));
printf("#define sfRippleEscrow %d\n", field_code( STI_AMOUNT, 17, "RippleEscrow"));
printf("#define sfDeliveredAmount %d\n", field_code( STI_AMOUNT, 18, "DeliveredAmount"));

// variable length (common)
printf("#define sfPublicKey %d\n", field_code( STI_VL, 1, "PublicKey"));
printf("#define sfMessageKey %d\n", field_code( STI_VL, 2, "MessageKey"));
printf("#define sfSigningPubKey %d\n", field_code( STI_VL, 3, "SigningPubKey"));
printf("#define sfTxnSignature %d\n", field_code(STI_VL, 4, "TxnSignature" ));
printf("#define sfSignature %d\n", field_code(STI_VL, 6, "Signature" ));
printf("#define sfDomain %d\n", field_code( STI_VL, 7, "Domain"));
printf("#define sfFundCode %d\n", field_code( STI_VL, 8, "FundCode"));
printf("#define sfRemoveCode %d\n", field_code( STI_VL, 9, "RemoveCode"));
printf("#define sfExpireCode %d\n", field_code( STI_VL, 10, "ExpireCode"));
printf("#define sfCreateCode %d\n", field_code( STI_VL, 11, "CreateCode"));
printf("#define sfMemoType %d\n", field_code( STI_VL, 12, "MemoType"));
printf("#define sfMemoData %d\n", field_code( STI_VL, 13, "MemoData"));
printf("#define sfMemoFormat %d\n", field_code( STI_VL, 14, "MemoFormat"));

// variable length (uncommon)
printf("#define sfFulfillment %d\n", field_code( STI_VL, 16, "Fulfillment"));
printf("#define sfCondition %d\n", field_code( STI_VL, 17, "Condition"));
printf("#define sfMasterSignature %d\n", field_code(STI_VL, 18, "MasterSignature"));
printf("#define sfUNLModifyValidator %d\n", field_code( STI_VL, 19, "UNLModifyValidator"));
printf("#define sfNegativeUNLToDisable %d\n", field_code( STI_VL, 20, "ValidatorToDisable"));
printf("#define sfNegativeUNLToReEnable %d\n", field_code( STI_VL, 21, "ValidatorToReEnable"));
printf("#define sfHookData %d\n", field_code( STI_VL, 22, "HookData"));

// account
printf("#define sfAccount %d\n", field_code( STI_ACCOUNT, 1, "Account"));
printf("#define sfOwner %d\n", field_code( STI_ACCOUNT, 2, "Owner"));
printf("#define sfDestination %d\n", field_code( STI_ACCOUNT, 3, "Destination"));
printf("#define sfIssuer %d\n", field_code( STI_ACCOUNT, 4, "Issuer"));
printf("#define sfAuthorize %d\n", field_code( STI_ACCOUNT, 5, "Authorize"));
printf("#define sfUnauthorize %d\n", field_code( STI_ACCOUNT, 6, "Unauthorize"));
printf("#define sfTarget %d\n", field_code( STI_ACCOUNT, 7, "Target"));
printf("#define sfRegularKey %d\n", field_code( STI_ACCOUNT, 8, "RegularKey"));

// path set
printf("#define sfPaths %d\n", field_code( STI_PATHSET, 1, "Paths"));

// vector of 256-bit
printf("#define sfIndexes %d\n", field_code( STI_VECTOR256, 1, "Indexes"));
printf("#define sfHashes %d\n", field_code( STI_VECTOR256, 2, "Hashes"));
printf("#define sfAmendments %d\n", field_code( STI_VECTOR256, 3, "Amendments"));

// inner object
// OBJECT/1 is reserved for end of object
printf("#define sfTransactionMetaData %d\n", field_code( STI_OBJECT, 2, "TransactionMetaData"));
printf("#define sfCreatedNode %d\n", field_code( STI_OBJECT, 3, "CreatedNode"));
printf("#define sfDeletedNode %d\n", field_code( STI_OBJECT, 4, "DeletedNode"));
printf("#define sfModifiedNode %d\n", field_code( STI_OBJECT, 5, "ModifiedNode"));
printf("#define sfPreviousFields %d\n", field_code( STI_OBJECT, 6, "PreviousFields"));
printf("#define sfFinalFields %d\n", field_code( STI_OBJECT, 7, "FinalFields"));
printf("#define sfNewFields %d\n", field_code( STI_OBJECT, 8, "NewFields"));
printf("#define sfTemplateEntry %d\n", field_code( STI_OBJECT, 9, "TemplateEntry"));
printf("#define sfMemo %d\n", field_code( STI_OBJECT, 10, "Memo"));
printf("#define sfSignerEntry %d\n", field_code( STI_OBJECT, 11, "SignerEntry"));

// inner object (uncommon)
printf("#define sfSigner %d\n", field_code( STI_OBJECT, 16, "Signer"));
//                                                                                 17 has not been used yet...
printf("#define sfMajority %d\n", field_code( STI_OBJECT, 18, "Majority"));
printf("#define sfNegativeUNLEntry %d\n", field_code( STI_OBJECT, 19, "DisabledValidator"));

// array of objects
// ARRAY/1 is reserved for end of array
printf("#define sfSigningAccounts  %d\n", field_code( STI_ARRAY, 2, "SigningAccounts")), //
// Never been used.
printf("#define sfSigners %d\n", field_code(STI_ARRAY, 3, "Signers"));
printf("#define sfSignerEntries %d\n", field_code( STI_ARRAY, 4, "SignerEntries"));
printf("#define sfTemplate %d\n", field_code( STI_ARRAY, 5, "Template"));
printf("#define sfNecessary %d\n", field_code( STI_ARRAY, 6, "Necessary"));
printf("#define sfSufficient %d\n", field_code( STI_ARRAY, 7, "Sufficient"));
printf("#define sfAffectedNodes %d\n", field_code( STI_ARRAY, 8, "AffectedNodes"));
printf("#define sfMemos %d\n", field_code( STI_ARRAY, 9, "Memos"));

// array of objects (uncommon)
printf("#define sfMajorities %d\n", field_code( STI_ARRAY, 16, "Majorities"));
printf("#define sfNegativeUNL %d\n", field_code( STI_ARRAY, 17, "NegativeUNL"));

return 0;
}
