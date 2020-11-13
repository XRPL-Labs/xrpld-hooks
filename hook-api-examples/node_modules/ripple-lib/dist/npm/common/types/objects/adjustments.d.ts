import { Amount } from './amounts';
export declare type Adjustment = {
    address: string;
    amount: Amount;
    tag?: number;
};
export declare type MaxAdjustment = {
    address: string;
    maxAmount: Amount;
    tag?: number;
};
export declare type MinAdjustment = {
    address: string;
    minAmount: Amount;
    tag?: number;
};
//# sourceMappingURL=adjustments.d.ts.map