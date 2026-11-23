import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as QuickSettings from 'resource:///org/gnome/shell/ui/quickSettings.js';

import {connectService, setEnabled} from './client.js';

const Indicator = GObject.registerClass(class Indicator extends QuickSettings.SystemIndicator {
    _init() {
        super._init();
        this._cancel = new Gio.Cancellable();
        this._toggle = new QuickSettings.QuickToggle({
            title: 'Tablet-only touchscreen',
            iconName: 'input-tablet-symbolic',
            toggleMode: false,
        });
        this.quickSettingsItems.push(this._toggle);
        this._toggle.connect('clicked', () => this._setEnabled());
        this._sync();
        this._connectService();
    }

    async _connectService() {
        try {
            const proxy = await connectService(this._cancel);
            if (this._cancel.is_cancelled())
                return;
            this._proxy = proxy;
            proxy.connectObject('g-properties-changed', () => this._sync(),
                'notify::g-name-owner', () => this._sync(), this);
            this._sync();
        } catch (e) {
            if (!this._cancel.is_cancelled())
                console.error(e);
        }
    }

    _sync() {
        const p = this._proxy;
        const get = k => p?.get_cached_property(k)?.deepUnpack();
        const online = Boolean(p?.g_name_owner && get('Enabled') !== undefined);
        const enabled = online && get('Enabled');
        const problem = get('Problem');
        this._toggle.checked = enabled;
        this._toggle.reactive = online && !this._busy && (enabled || !problem);
        this._toggle.can_focus = this._toggle.reactive;
        this._toggle.subtitle = !online ? 'Service unavailable' : problem ||
            (!enabled ? 'Off' : get('Blocked') ? 'Laptop mode · touch blocked' :
                'Tablet mode · touch available');
    }

    async _setEnabled() {
        if (!this._toggle.reactive)
            return;
        this._busy = true;
        this._sync();
        try {
            await setEnabled(this._proxy, !this._toggle.checked, this._cancel);
        } catch (e) {
            if (!this._cancel.is_cancelled())
                Main.notifyError('Tablet-only touchscreen', e.message);
        } finally {
            this._busy = false;
            if (!this._cancel.is_cancelled())
                this._sync();
        }
    }

    destroy() {
        this._cancel.cancel();
        this._proxy?.disconnectObject(this);
        this._toggle.destroy();
        super.destroy();
    }
});

export default class TabletOnlyTouchscreen extends Extension {
    enable() {
        this._indicator = new Indicator();
        Main.panel.statusArea.quickSettings.addExternalIndicator(this._indicator);
    }

    disable() {
        this._indicator.destroy();
        this._indicator = null;
    }
}
