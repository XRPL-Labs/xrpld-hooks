import { PayChannelLedgerEntry } from '../../common/types/objects';
export declare type FormattedPaymentChannel = {
    account: string;
    amount: string;
    balance: string;
    publicKey: string;
    destination: string;
    settleDelay: number;
    expiration?: string;
    cancelAfter?: string;
    sourceTag?: number;
    destinationTag?: number;
    previousAffectingTransactionID: string;
    previousAffectingTransactionLedgerVersion: number;
};
export declare function parsePaymentChannel(data: PayChannelLedgerEntry): FormattedPaymentChannel;
//# sourceMappingURL=payment-channel.d.ts.map