export declare class ExponentialBackoff {
    private readonly ms;
    private readonly max;
    private readonly factor;
    private readonly jitter;
    attempts: number;
    constructor(opts?: {
        min?: number;
        max?: number;
    });
    duration(): number;
    reset(): void;
}
//# sourceMappingURL=backoff.d.ts.map