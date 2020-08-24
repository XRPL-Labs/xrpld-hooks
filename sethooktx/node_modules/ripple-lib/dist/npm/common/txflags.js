"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const txFlags = {
    Universal: {
        FullyCanonicalSig: 0x80000000
    },
    AccountSet: {
        RequireDestTag: 0x00010000,
        OptionalDestTag: 0x00020000,
        RequireAuth: 0x00040000,
        OptionalAuth: 0x00080000,
        DisallowXRP: 0x00100000,
        AllowXRP: 0x00200000
    },
    TrustSet: {
        SetAuth: 0x00010000,
        NoRipple: 0x00020000,
        SetNoRipple: 0x00020000,
        ClearNoRipple: 0x00040000,
        SetFreeze: 0x00100000,
        ClearFreeze: 0x00200000
    },
    OfferCreate: {
        Passive: 0x00010000,
        ImmediateOrCancel: 0x00020000,
        FillOrKill: 0x00040000,
        Sell: 0x00080000
    },
    Payment: {
        NoRippleDirect: 0x00010000,
        PartialPayment: 0x00020000,
        LimitQuality: 0x00040000
    },
    PaymentChannelClaim: {
        Renew: 0x00010000,
        Close: 0x00020000
    }
};
exports.txFlags = txFlags;
const txFlagIndices = {
    AccountSet: {
        asfRequireDest: 1,
        asfRequireAuth: 2,
        asfDisallowXRP: 3,
        asfDisableMaster: 4,
        asfAccountTxnID: 5,
        asfNoFreeze: 6,
        asfGlobalFreeze: 7,
        asfDefaultRipple: 8,
        asfDepositAuth: 9
    }
};
exports.txFlagIndices = txFlagIndices;
//# sourceMappingURL=txflags.js.map