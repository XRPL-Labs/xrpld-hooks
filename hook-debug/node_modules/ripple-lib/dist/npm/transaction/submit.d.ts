import { RippleAPI } from '..';
export interface FormattedSubmitResponse {
    resultCode: string;
    resultMessage: string;
}
declare function submit(this: RippleAPI, signedTransaction: string, failHard?: boolean): Promise<FormattedSubmitResponse>;
export default submit;
//# sourceMappingURL=submit.d.ts.map