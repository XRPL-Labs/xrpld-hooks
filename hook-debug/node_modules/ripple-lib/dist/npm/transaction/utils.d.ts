import * as common from '../common';
import { Memo } from '../common/types/objects';
import { Instructions, Prepare, TransactionJSON } from './types';
import { RippleAPI } from '..';
export declare type ApiMemo = {
    MemoData?: string;
    MemoType?: string;
    MemoFormat?: string;
};
declare function setCanonicalFlag(txJSON: TransactionJSON): void;
export interface ClassicAccountAndTag {
    classicAccount: string;
    tag: number | false | undefined;
}
declare function getClassicAccountAndTag(Account: string, expectedTag?: number): ClassicAccountAndTag;
declare function prepareTransaction(txJSON: TransactionJSON, api: RippleAPI, instructions: Instructions): Promise<Prepare>;
declare function convertStringToHex(string: string): string;
declare function convertMemo(memo: Memo): {
    Memo: ApiMemo;
};
export { convertStringToHex, convertMemo, prepareTransaction, common, setCanonicalFlag, getClassicAccountAndTag };
//# sourceMappingURL=utils.d.ts.map