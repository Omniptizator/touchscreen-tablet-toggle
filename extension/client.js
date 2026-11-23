import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

const NAME = 'io.github.TouchscreenTabletToggle';
const PATH = '/io/github/TouchscreenTabletToggle';

export function connectService(cancellable) {
    return new Promise((resolve, reject) => {
        Gio.DBusProxy.new_for_bus(Gio.BusType.SYSTEM,
            Gio.DBusProxyFlags.DO_NOT_AUTO_START, null, NAME, PATH, NAME,
            cancellable, (_source, result) => {
                try {
                    resolve(Gio.DBusProxy.new_for_bus_finish(result));
                } catch (e) {
                    reject(e);
                }
            });
    });
}

export function setEnabled(proxy, enabled, cancellable) {
    return new Promise((resolve, reject) => {
        proxy.call('SetEnabled', new GLib.Variant('(b)', [enabled]),
            Gio.DBusCallFlags.NONE, 10000, cancellable, (source, result) => {
                try {
                    resolve(source.call_finish(result));
                } catch (e) {
                    reject(e);
                }
            });
    });
}
