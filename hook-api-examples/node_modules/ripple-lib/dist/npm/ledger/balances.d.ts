import { GetTrustlinesOptions } from './trustlines';
import { RippleAPI } from '..';
export declare type Balance = {
    value: string;
    currency: string;
    counterparty?: string;
};
export declare type GetBalances = Array<Balance>;
declare function getBalances(this: RippleAPI, address: string, options?: GetTrustlinesOptions): Promise<GetBalances>;
export default getBalances;
//# sourceMappingURL=balances.d.ts.map