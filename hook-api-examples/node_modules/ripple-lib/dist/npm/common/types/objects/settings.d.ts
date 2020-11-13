import { Memo } from './memos';
export declare type WeightedSigner = {
    address: string;
    weight: number;
};
export declare type Signers = {
    threshold?: number;
    weights: WeightedSigner[];
};
export declare type FormattedSettings = {
    defaultRipple?: boolean;
    depositAuth?: boolean;
    disableMasterKey?: boolean;
    disallowIncomingXRP?: boolean;
    domain?: string;
    emailHash?: string | null;
    walletLocator?: string | null;
    enableTransactionIDTracking?: boolean;
    globalFreeze?: boolean;
    memos?: Memo[];
    messageKey?: string;
    noFreeze?: boolean;
    passwordSpent?: boolean;
    regularKey?: string;
    requireAuthorization?: boolean;
    requireDestinationTag?: boolean;
    signers?: Signers;
    transferRate?: number | null;
    tickSize?: number;
};
//# sourceMappingURL=settings.d.ts.map