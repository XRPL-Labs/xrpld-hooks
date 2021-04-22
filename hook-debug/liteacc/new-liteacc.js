const keypairs = require('ripple-keypairs')
let seed = keypairs.generateSeed({algorithm: "ed25519"})
let keys = keypairs.deriveKeypair(seed);
console.log({ publicKey: keys.publicKey, seed: seed } );
