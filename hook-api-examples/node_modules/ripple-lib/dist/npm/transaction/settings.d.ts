import { Instructions, Prepare } from './types';
import { FormattedSettings } from '../common/types/objects';
import { RippleAPI } from '..';
declare function prepareSettings(this: RippleAPI, address: string, settings: FormattedSettings, instructions?: Instructions): Promise<Prepare>;
export default prepareSettings;
//# sourceMappingURL=settings.d.ts.map