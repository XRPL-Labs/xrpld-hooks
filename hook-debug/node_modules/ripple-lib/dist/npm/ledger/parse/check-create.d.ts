import { Amount } from '../../common/types/objects';
export declare type FormattedCheckCreate = {
    destination: string;
    sendMax: Amount;
    destinationTag?: string;
    expiration?: string;
    invoiceID?: string;
};
declare function parseCheckCreate(tx: any): FormattedCheckCreate;
export default parseCheckCreate;
//# sourceMappingURL=check-create.d.ts.map