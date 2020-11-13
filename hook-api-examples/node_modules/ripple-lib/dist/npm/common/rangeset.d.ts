declare class RangeSet {
    ranges: Array<[number, number]>;
    constructor();
    reset(): void;
    serialize(): string;
    addRange(start: number, end: number): void;
    addValue(value: number): void;
    parseAndAddRanges(rangesString: string): void;
    containsRange(start: number, end: number): boolean;
    containsValue(value: number): boolean;
}
export default RangeSet;
//# sourceMappingURL=rangeset.d.ts.map