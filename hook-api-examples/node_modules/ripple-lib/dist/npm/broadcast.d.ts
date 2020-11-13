import { RippleAPI, APIOptions } from './api';
declare class RippleAPIBroadcast extends RippleAPI {
    ledgerVersion: number | undefined;
    private _apis;
    constructor(servers: any, options?: APIOptions);
    onLedgerEvent(ledger: any): void;
    getMethodNames(): string[];
}
export { RippleAPIBroadcast };
//# sourceMappingURL=broadcast.d.ts.map