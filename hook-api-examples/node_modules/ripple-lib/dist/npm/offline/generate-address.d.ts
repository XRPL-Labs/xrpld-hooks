export declare type GeneratedAddress = {
    xAddress: string;
    classicAddress?: string;
    address?: string;
    secret: string;
};
export interface GenerateAddressOptions {
    entropy?: Uint8Array | number[];
    algorithm?: 'ecdsa-secp256k1' | 'ed25519';
    test?: boolean;
    includeClassicAddress?: boolean;
}
declare function generateAddressAPI(options: GenerateAddressOptions): GeneratedAddress;
export { generateAddressAPI };
//# sourceMappingURL=generate-address.d.ts.map