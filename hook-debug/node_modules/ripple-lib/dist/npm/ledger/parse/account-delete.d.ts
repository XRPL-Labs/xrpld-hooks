export declare type FormattedAccountDelete = {
    destination: string;
    destinationTag?: number;
    destinationXAddress: string;
};
declare function parseAccountDelete(tx: any): FormattedAccountDelete;
export default parseAccountDelete;
//# sourceMappingURL=account-delete.d.ts.map