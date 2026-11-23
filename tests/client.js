import {connectService, setEnabled} from '../extension/client.js';

const proxy = await connectService(null);
if (proxy.get_cached_property('TabletMode').deepUnpack() !== 0)
    throw Error('Missing initial posture');
await setEnabled(proxy, true, null);
if (!proxy.get_cached_property('Blocked').deepUnpack())
    throw Error('Missing status update');
