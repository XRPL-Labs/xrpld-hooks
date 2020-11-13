import { Instructions, Prepare } from './types';
import { RippleAPI } from '..';
export declare type CheckCancelParameters = {
    checkID: string;
};
declare function prepareCheckCancel(this: RippleAPI, address: string, checkCancel: CheckCancelParameters, instructions?: Instructions): Promise<Prepare>;
export default prepareCheckCancel;
//# sourceMappingURL=check-cancel.d.ts.map