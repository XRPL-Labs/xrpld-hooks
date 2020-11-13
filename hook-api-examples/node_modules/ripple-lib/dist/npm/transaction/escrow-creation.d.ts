import { Instructions, Prepare } from './types';
import { Memo } from '../common/types/objects';
import { RippleAPI } from '..';
export declare type EscrowCreation = {
    amount: string;
    destination: string;
    memos?: Array<Memo>;
    condition?: string;
    allowCancelAfter?: string;
    allowExecuteAfter?: string;
    sourceTag?: number;
    destinationTag?: number;
};
declare function prepareEscrowCreation(this: RippleAPI, address: string, escrowCreation: EscrowCreation, instructions?: Instructions): Promise<Prepare>;
export default prepareEscrowCreation;
//# sourceMappingURL=escrow-creation.d.ts.map