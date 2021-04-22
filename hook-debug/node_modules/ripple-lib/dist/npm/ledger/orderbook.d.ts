import { FormattedOrderbookOrder } from './parse/orderbook-order';
import { Issue } from '../common/types/objects';
import { BookOffer } from '../common/types/commands';
import { RippleAPI } from '..';
export declare type FormattedOrderbook = {
    bids: FormattedOrderbookOrder[];
    asks: FormattedOrderbookOrder[];
};
export declare function formatBidsAndAsks(orderbook: OrderbookInfo, offers: BookOffer[]): {
    bids: FormattedOrderbookOrder[];
    asks: FormattedOrderbookOrder[];
};
export declare type GetOrderbookOptions = {
    limit?: number;
    ledgerVersion?: number;
};
export declare type OrderbookInfo = {
    base: Issue;
    counter: Issue;
};
export declare function getOrderbook(this: RippleAPI, address: string, orderbook: OrderbookInfo, options?: GetOrderbookOptions): Promise<FormattedOrderbook>;
//# sourceMappingURL=orderbook.d.ts.map