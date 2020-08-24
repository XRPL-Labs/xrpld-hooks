import { Instructions, Prepare } from './types';
import { Memo } from '../common/types/objects';
import { RippleAPI } from '..';
export declare type EscrowCancellation = {
    owner: string;
    escrowSequence: number;
    memos?: Array<Memo>;
};
declare function prepareEscrowCancellation(this: RippleAPI, address: string, escrowCancellation: EscrowCancellation, instructions?: Instructions): Promise<Prepare>;
export default prepareEscrowCancellation;
//# sourceMappingURL=escrow-cancellation.d.ts.map