import { FormattedLedger } from './parse/ledger';
import { RippleAPI } from '..';
export declare type GetLedgerOptions = {
    ledgerHash?: string;
    ledgerVersion?: number;
    includeAllData?: boolean;
    includeTransactions?: boolean;
    includeState?: boolean;
};
declare function getLedger(this: RippleAPI, options?: GetLedgerOptions): Promise<FormattedLedger>;
export default getLedger;
//# sourceMappingURL=ledger.d.ts.map