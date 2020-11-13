import { Instructions, Prepare } from './types';
import { FormattedOrderSpecification } from '../common/types/objects/index';
import { RippleAPI } from '..';
declare function prepareOrder(this: RippleAPI, address: string, order: FormattedOrderSpecification, instructions?: Instructions): Promise<Prepare>;
export default prepareOrder;
//# sourceMappingURL=order.d.ts.map