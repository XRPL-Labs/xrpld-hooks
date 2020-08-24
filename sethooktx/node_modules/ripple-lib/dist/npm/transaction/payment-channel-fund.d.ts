import { Instructions, Prepare } from './types';
import { RippleAPI } from '..';
export declare type PaymentChannelFund = {
    channel: string;
    amount: string;
    expiration?: string;
};
declare function preparePaymentChannelFund(this: RippleAPI, address: string, paymentChannelFund: PaymentChannelFund, instructions?: Instructions): Promise<Prepare>;
export default preparePaymentChannelFund;
//# sourceMappingURL=payment-channel-fund.d.ts.map