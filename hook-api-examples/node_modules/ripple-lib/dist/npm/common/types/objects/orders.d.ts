import { Amount } from './amounts';
import { Memo } from './memos';
export declare type FormattedOrderSpecification = {
    direction: string;
    quantity: Amount;
    totalPrice: Amount;
    immediateOrCancel?: boolean;
    fillOrKill?: boolean;
    expirationTime?: string;
    orderToReplace?: number;
    memos?: Memo[];
    passive?: boolean;
};
//# sourceMappingURL=orders.d.ts.map