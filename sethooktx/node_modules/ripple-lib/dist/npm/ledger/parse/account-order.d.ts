import { FormattedOrderSpecification } from '../../common/types/objects';
export declare type FormattedAccountOrder = {
    specification: FormattedOrderSpecification;
    properties: {
        maker: string;
        sequence: number;
        makerExchangeRate: string;
    };
};
export declare function parseAccountOrder(address: string, order: any): FormattedAccountOrder;
//# sourceMappingURL=account-order.d.ts.map