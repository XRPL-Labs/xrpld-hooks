import { Instructions, Prepare } from './types';
import { RippleAPI } from '..';
export declare type PaymentChannelCreate = {
    amount: string;
    destination: string;
    settleDelay: number;
    publicKey: string;
    cancelAfter?: string;
    sourceTag?: number;
    destinationTag?: number;
};
declare function preparePaymentChannelCreate(this: RippleAPI, address: string, paymentChannelCreate: PaymentChannelCreate, instructions?: Instructions): Promise<Prepare>;
export default preparePaymentChannelCreate;
//# sourceMappingURL=payment-channel-create.d.ts.map