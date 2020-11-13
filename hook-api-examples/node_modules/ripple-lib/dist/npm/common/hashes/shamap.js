"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const hash_prefix_1 = __importDefault(require("./hash-prefix"));
const sha512Half_1 = __importDefault(require("./sha512Half"));
const HEX_ZERO = '0000000000000000000000000000000000000000000000000000000000000000';
var NodeType;
(function (NodeType) {
    NodeType[NodeType["INNER"] = 1] = "INNER";
    NodeType[NodeType["TRANSACTION_NO_METADATA"] = 2] = "TRANSACTION_NO_METADATA";
    NodeType[NodeType["TRANSACTION_METADATA"] = 3] = "TRANSACTION_METADATA";
    NodeType[NodeType["ACCOUNT_STATE"] = 4] = "ACCOUNT_STATE";
})(NodeType = exports.NodeType || (exports.NodeType = {}));
class Node {
    constructor() { }
    addItem(_tag, _node) {
        throw new Error('Called unimplemented virtual method SHAMapTreeNode#addItem.');
    }
    get hash() {
        throw new Error('Called unimplemented virtual method SHAMapTreeNode#hash.');
    }
}
exports.Node = Node;
class InnerNode extends Node {
    constructor(depth = 0) {
        super();
        this.leaves = {};
        this.type = NodeType.INNER;
        this.depth = depth;
        this.empty = true;
    }
    addItem(tag, node) {
        const existingNode = this.getNode(parseInt(tag[this.depth], 16));
        if (existingNode) {
            if (existingNode instanceof InnerNode) {
                existingNode.addItem(tag, node);
            }
            else if (existingNode instanceof Leaf) {
                if (existingNode.tag === tag) {
                    throw new Error('Tried to add a node to a SHAMap that was already in there.');
                }
                else {
                    const newInnerNode = new InnerNode(this.depth + 1);
                    newInnerNode.addItem(existingNode.tag, existingNode);
                    newInnerNode.addItem(tag, node);
                    this.setNode(parseInt(tag[this.depth], 16), newInnerNode);
                }
            }
        }
        else {
            this.setNode(parseInt(tag[this.depth], 16), node);
        }
    }
    setNode(slot, node) {
        if (slot < 0 || slot > 15) {
            throw new Error('Invalid slot: slot must be between 0-15.');
        }
        this.leaves[slot] = node;
        this.empty = false;
    }
    getNode(slot) {
        if (slot < 0 || slot > 15) {
            throw new Error('Invalid slot: slot must be between 0-15.');
        }
        return this.leaves[slot];
    }
    get hash() {
        if (this.empty)
            return HEX_ZERO;
        let hex = '';
        for (let i = 0; i < 16; i++) {
            hex += this.leaves[i] ? this.leaves[i].hash : HEX_ZERO;
        }
        const prefix = hash_prefix_1.default.INNER_NODE.toString(16);
        return sha512Half_1.default(prefix + hex);
    }
}
exports.InnerNode = InnerNode;
class Leaf extends Node {
    constructor(tag, data, type) {
        super();
        this.tag = tag;
        this.type = type;
        this.data = data;
    }
    get hash() {
        switch (this.type) {
            case NodeType.ACCOUNT_STATE: {
                const leafPrefix = hash_prefix_1.default.LEAF_NODE.toString(16);
                return sha512Half_1.default(leafPrefix + this.data + this.tag);
            }
            case NodeType.TRANSACTION_NO_METADATA: {
                const txIDPrefix = hash_prefix_1.default.TRANSACTION_ID.toString(16);
                return sha512Half_1.default(txIDPrefix + this.data);
            }
            case NodeType.TRANSACTION_METADATA: {
                const txNodePrefix = hash_prefix_1.default.TRANSACTION_NODE.toString(16);
                return sha512Half_1.default(txNodePrefix + this.data + this.tag);
            }
            default:
                throw new Error('Tried to hash a SHAMap node of unknown type.');
        }
    }
}
exports.Leaf = Leaf;
class SHAMap {
    constructor() {
        this.root = new InnerNode(0);
    }
    addItem(tag, data, type) {
        this.root.addItem(tag, new Leaf(tag, data, type));
    }
    get hash() {
        return this.root.hash;
    }
}
exports.SHAMap = SHAMap;
//# sourceMappingURL=shamap.js.map