import { FormattedAccountOrder } from './parse/account-order';
import { RippleAPI } from '..';
export declare type GetOrdersOptions = {
    limit?: number;
    ledgerVersion?: number;
};
export default function getOrders(this: RippleAPI, address: string, options?: GetOrdersOptions): Promise<FormattedAccountOrder[]>;
//# sourceMappingURL=orders.d.ts.map