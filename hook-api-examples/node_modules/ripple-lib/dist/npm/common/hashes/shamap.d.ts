export declare enum NodeType {
    INNER = 1,
    TRANSACTION_NO_METADATA = 2,
    TRANSACTION_METADATA = 3,
    ACCOUNT_STATE = 4
}
export declare abstract class Node {
    constructor();
    addItem(_tag: string, _node: Node): void;
    get hash(): string | void;
}
export declare class InnerNode extends Node {
    leaves: {
        [slot: number]: Node;
    };
    type: NodeType;
    depth: number;
    empty: boolean;
    constructor(depth?: number);
    addItem(tag: string, node: Node): void;
    setNode(slot: number, node: Node): void;
    getNode(slot: number): Node;
    get hash(): string;
}
export declare class Leaf extends Node {
    tag: string;
    type: NodeType;
    data: string;
    constructor(tag: string, data: string, type: NodeType);
    get hash(): string | void;
}
export declare class SHAMap {
    root: InnerNode;
    constructor();
    addItem(tag: string, data: string, type: NodeType): void;
    get hash(): string;
}
//# sourceMappingURL=shamap.d.ts.map