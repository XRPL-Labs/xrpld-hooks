import { Instructions, Prepare } from './types';
import { Amount } from '../common/types/objects';
import { RippleAPI } from '..';
export declare type CheckCreateParameters = {
    destination: string;
    sendMax: Amount;
    destinationTag?: number;
    expiration?: string;
    invoiceID?: string;
};
declare function prepareCheckCreate(this: RippleAPI, address: string, checkCreate: CheckCreateParameters, instructions?: Instructions): Promise<Prepare>;
export default prepareCheckCreate;
//# sourceMappingURL=check-create.d.ts.map