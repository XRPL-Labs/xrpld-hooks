declare const txFlags: {
    Universal: {
        FullyCanonicalSig: number;
    };
    AccountSet: {
        RequireDestTag: number;
        OptionalDestTag: number;
        RequireAuth: number;
        OptionalAuth: number;
        DisallowXRP: number;
        AllowXRP: number;
    };
    TrustSet: {
        SetAuth: number;
        NoRipple: number;
        SetNoRipple: number;
        ClearNoRipple: number;
        SetFreeze: number;
        ClearFreeze: number;
    };
    OfferCreate: {
        Passive: number;
        ImmediateOrCancel: number;
        FillOrKill: number;
        Sell: number;
    };
    Payment: {
        NoRippleDirect: number;
        PartialPayment: number;
        LimitQuality: number;
    };
    PaymentChannelClaim: {
        Renew: number;
        Close: number;
    };
};
declare const txFlagIndices: {
    AccountSet: {
        asfRequireDest: number;
        asfRequireAuth: number;
        asfDisallowXRP: number;
        asfDisableMaster: number;
        asfAccountTxnID: number;
        asfNoFreeze: number;
        asfGlobalFreeze: number;
        asfDefaultRipple: number;
        asfDepositAuth: number;
    };
};
export { txFlags, txFlagIndices };
//# sourceMappingURL=txflags.d.ts.map