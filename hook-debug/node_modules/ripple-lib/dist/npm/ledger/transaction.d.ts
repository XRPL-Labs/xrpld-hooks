import { FormattedTransactionType } from '../transaction/types';
import { RippleAPI } from '..';
export declare type TransactionOptions = {
    minLedgerVersion?: number;
    maxLedgerVersion?: number;
    includeRawTransaction?: boolean;
};
declare function getTransaction(this: RippleAPI, id: string, options?: TransactionOptions): Promise<FormattedTransactionType>;
export default getTransaction;
//# sourceMappingURL=transaction.d.ts.map