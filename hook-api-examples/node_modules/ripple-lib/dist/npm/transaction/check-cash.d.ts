import { Instructions, Prepare } from './types';
import { Amount } from '../common/types/objects';
import { RippleAPI } from '..';
export declare type CheckCashParameters = {
    checkID: string;
    amount?: Amount;
    deliverMin?: Amount;
};
declare function prepareCheckCash(this: RippleAPI, address: string, checkCash: CheckCashParameters, instructions?: Instructions): Promise<Prepare>;
export default prepareCheckCash;
//# sourceMappingURL=check-cash.d.ts.map