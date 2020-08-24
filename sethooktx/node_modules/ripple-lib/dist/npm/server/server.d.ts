import { RippleAPI } from '..';
declare function isConnected(this: RippleAPI): boolean;
declare function getLedgerVersion(this: RippleAPI): Promise<number>;
declare function connect(this: RippleAPI): Promise<void>;
declare function disconnect(this: RippleAPI): Promise<void>;
declare function formatLedgerClose(ledgerClose: any): object;
export { connect, disconnect, isConnected, getLedgerVersion, formatLedgerClose };
//# sourceMappingURL=server.d.ts.map