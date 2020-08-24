import { Ledger } from '../../common/types/objects';
export declare type FormattedLedger = {
    stateHash: string;
    closeTime: string;
    closeTimeResolution: number;
    closeFlags: number;
    ledgerHash: string;
    ledgerVersion: number;
    parentLedgerHash: string;
    parentCloseTime: string;
    totalDrops: string;
    transactionHash: string;
    transactions?: Array<object>;
    transactionHashes?: Array<string>;
    rawState?: string;
    stateHashes?: Array<string>;
};
export declare function parseLedger(ledger: Ledger): FormattedLedger;
//# sourceMappingURL=ledger.d.ts.map