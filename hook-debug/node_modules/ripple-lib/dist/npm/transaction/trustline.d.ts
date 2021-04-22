import { Instructions, Prepare } from './types';
import { FormattedTrustlineSpecification } from '../common/types/objects/trustlines';
import { RippleAPI } from '..';
declare function prepareTrustline(this: RippleAPI, address: string, trustline: FormattedTrustlineSpecification, instructions?: Instructions): Promise<Prepare>;
export default prepareTrustline;
//# sourceMappingURL=trustline.d.ts.map