import * as _ from 'lodash';
import * as common from '../common';
import { Connection } from '../common';
import { FormattedTransactionType } from '../transaction/types';
import { Issue } from '../common/types/objects';
import { RippleAPI } from '..';
export declare type RecursiveData = {
    marker: string;
    results: Array<any>;
};
export declare type Getter = (marker?: string, limit?: number) => Promise<RecursiveData>;
declare function clamp(value: number, min: number, max: number): number;
declare function getXRPBalance(connection: Connection, address: string, ledgerVersion?: number): Promise<string>;
declare function getRecursive(getter: Getter, limit?: number): Promise<Array<any>>;
declare function renameCounterpartyToIssuer<T>(obj: T & {
    counterparty?: string;
    issuer?: string;
}): T & {
    issuer?: string;
};
export declare type RequestBookOffersArgs = {
    taker_gets: Issue;
    taker_pays: Issue;
};
declare function renameCounterpartyToIssuerInOrder(order: RequestBookOffersArgs): RequestBookOffersArgs & _.Dictionary<Issue & {
    issuer?: string;
}>;
declare function compareTransactions(first: FormattedTransactionType, second: FormattedTransactionType): number;
declare function hasCompleteLedgerRange(connection: Connection, minLedgerVersion?: number, maxLedgerVersion?: number): Promise<boolean>;
declare function isPendingLedgerVersion(connection: Connection, maxLedgerVersion?: number): Promise<boolean>;
declare function ensureLedgerVersion(this: RippleAPI, options: any): Promise<object>;
export { getXRPBalance, ensureLedgerVersion, compareTransactions, renameCounterpartyToIssuer, renameCounterpartyToIssuerInOrder, getRecursive, hasCompleteLedgerRange, isPendingLedgerVersion, clamp, common };
//# sourceMappingURL=utils.d.ts.map