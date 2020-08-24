/// <reference types="node" />
import { EventEmitter } from 'events';
declare class WSWrapper extends EventEmitter {
    private _ws;
    static CONNECTING: number;
    static OPEN: number;
    static CLOSING: number;
    static CLOSED: number;
    constructor(url: any, _protocols: any, _websocketOptions: any);
    close(): void;
    send(message: any): void;
    get readyState(): number;
}
export = WSWrapper;
//# sourceMappingURL=wswrapper.d.ts.map