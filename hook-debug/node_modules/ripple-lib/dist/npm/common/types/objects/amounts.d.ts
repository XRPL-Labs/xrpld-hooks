export interface Amount extends Issue {
    value: string;
}
export declare type RippledAmount = string | Amount;
export interface TakerRequestAmount {
    currency: string;
    issuer?: string;
}
export interface Issue {
    currency: string;
    issuer?: string;
    counterparty?: string;
}
//# sourceMappingURL=amounts.d.ts.map