import { RippleAPI } from '..';
export declare type GetAccountInfoOptions = {
    ledgerVersion?: number;
};
export declare type FormattedGetAccountInfoResponse = {
    sequence: number;
    xrpBalance: string;
    ownerCount: number;
    previousInitiatedTransactionID: string;
    previousAffectingTransactionID: string;
    previousAffectingTransactionLedgerVersion: number;
};
export default function getAccountInfo(this: RippleAPI, address: string, options?: GetAccountInfoOptions): Promise<FormattedGetAccountInfoResponse>;
//# sourceMappingURL=accountinfo.d.ts.map