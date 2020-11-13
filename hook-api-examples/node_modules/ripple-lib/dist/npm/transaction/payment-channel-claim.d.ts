import { Instructions, Prepare } from './types';
import { RippleAPI } from '..';
export declare type PaymentChannelClaim = {
    channel: string;
    balance?: string;
    amount?: string;
    signature?: string;
    publicKey?: string;
    renew?: boolean;
    close?: boolean;
};
declare function preparePaymentChannelClaim(this: RippleAPI, address: string, paymentChannelClaim: PaymentChannelClaim, instructions?: Instructions): Promise<Prepare>;
export default preparePaymentChannelClaim;
//# sourceMappingURL=payment-channel-claim.d.ts.map