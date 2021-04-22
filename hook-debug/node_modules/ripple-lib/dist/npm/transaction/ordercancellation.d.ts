import { Instructions, Prepare } from './types';
import { RippleAPI } from '..';
declare function prepareOrderCancellation(this: RippleAPI, address: string, orderCancellation: object, instructions?: Instructions): Promise<Prepare>;
export default prepareOrderCancellation;
//# sourceMappingURL=ordercancellation.d.ts.map