import { RippleAPI } from '..';
export declare type GetServerInfoResponse = {
    buildVersion: string;
    completeLedgers: string;
    hostID: string;
    ioLatencyMs: number;
    load?: {
        jobTypes: Array<object>;
        threads: number;
    };
    lastClose: {
        convergeTimeS: number;
        proposers: number;
    };
    loadFactor: number;
    peers: number;
    pubkeyNode: string;
    pubkeyValidator?: string;
    serverState: string;
    validatedLedger: {
        age: number;
        baseFeeXRP: string;
        hash: string;
        reserveBaseXRP: string;
        reserveIncrementXRP: string;
        ledgerVersion: number;
    };
    validationQuorum: number;
    networkLedger?: string;
};
declare function getServerInfo(this: RippleAPI): Promise<GetServerInfoResponse>;
declare function getFee(this: RippleAPI, cushion?: number): Promise<string>;
export { getServerInfo, getFee };
//# sourceMappingURL=serverinfo.d.ts.map