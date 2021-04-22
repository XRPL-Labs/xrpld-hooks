import { Amount, RippledAmount, Adjustment, MaxAdjustment, MinAdjustment } from '../common/types/objects';
export declare type LaxLaxAmount = {
    currency: string;
    value?: string;
    issuer?: string;
    counterparty?: string;
};
export declare type Path = {
    source: Adjustment | MaxAdjustment;
    destination: Adjustment | MinAdjustment;
    paths: string;
};
export declare type GetPaths = Array<Path>;
export declare type PathFind = {
    source: {
        address: string;
        amount?: Amount;
        currencies?: Array<{
            currency: string;
            counterparty?: string;
        }>;
    };
    destination: {
        address: string;
        amount: LaxLaxAmount;
    };
};
export declare type PathFindRequest = {
    command: string;
    source_account: string;
    destination_amount: RippledAmount;
    destination_account: string;
    source_currencies?: {
        currency: string;
        issuer?: string;
    }[];
    send_max?: RippledAmount;
};
export declare type RippledPathsResponse = {
    alternatives: Array<{
        paths_computed: Array<Array<{
            type: number;
            type_hex: string;
            account?: string;
            issuer?: string;
            currency?: string;
        }>>;
        source_amount: RippledAmount;
    }>;
    type: string;
    destination_account: string;
    destination_amount: RippledAmount;
    destination_currencies?: Array<string>;
    source_account: string;
    source_currencies?: Array<{
        currency: string;
    }>;
    full_reply?: boolean;
};
//# sourceMappingURL=pathfind-types.d.ts.map