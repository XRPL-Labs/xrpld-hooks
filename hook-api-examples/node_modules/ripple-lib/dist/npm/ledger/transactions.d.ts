import { FormattedTransactionType } from '../transaction/types';
import { RippleAPI } from '..';
export declare type TransactionsOptions = {
    start?: string;
    limit?: number;
    minLedgerVersion?: number;
    maxLedgerVersion?: number;
    earliestFirst?: boolean;
    excludeFailures?: boolean;
    initiated?: boolean;
    counterparty?: string;
    types?: Array<string>;
    includeRawTransactions?: boolean;
    binary?: boolean;
    startTx?: FormattedTransactionType;
};
export declare type GetTransactionsResponse = Array<FormattedTransactionType>;
declare function getTransactions(this: RippleAPI, address: string, options?: TransactionsOptions): Promise<GetTransactionsResponse>;
export default getTransactions;
//# sourceMappingURL=transactions.d.ts.map