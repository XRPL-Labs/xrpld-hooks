import { BookOffer } from '../../common/types/commands';
import { Amount, FormattedOrderSpecification } from '../../common/types/objects';
export declare type FormattedOrderbookOrder = {
    specification: FormattedOrderSpecification;
    properties: {
        maker: string;
        sequence: number;
        makerExchangeRate: string;
    };
    state?: {
        fundedAmount: Amount;
        priceOfFundedAmount: Amount;
    };
    data: BookOffer;
};
export declare function parseOrderbookOrder(data: BookOffer): FormattedOrderbookOrder;
//# sourceMappingURL=orderbook-order.d.ts.map