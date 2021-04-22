import { Instructions, Prepare } from './types';
import { Memo } from '../common/types/objects';
import { RippleAPI } from '..';
export declare type EscrowExecution = {
    owner: string;
    escrowSequence: number;
    memos?: Array<Memo>;
    condition?: string;
    fulfillment?: string;
};
declare function prepareEscrowExecution(this: RippleAPI, address: string, escrowExecution: EscrowExecution, instructions?: Instructions): Promise<Prepare>;
export default prepareEscrowExecution;
//# sourceMappingURL=escrow-execution.d.ts.map