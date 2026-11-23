// Test-only extension, loaded in the disposable compositor, never in the RPM.
import GLib from 'gi://GLib';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

export default class Probe extends Extension {
    enable() {
        const dir = GLib.getenv('TTT_FIXTURE');
        const uuid = 'tablet-only-touchscreen@touchscreen-tablet-toggle';
        let phase = 0;
        this._timer = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 100, () => {
            const tile = Main.extensionManager.lookup(uuid)?.stateObj?._indicator?._toggle;
            if (!tile?.reactive)
                return GLib.SOURCE_CONTINUE;
            if (phase === 0 && !tile.checked && tile.subtitle === 'Off') {
                phase++;
                tile.emit('clicked', 1);
            } else if (phase === 1 && tile.checked && tile.subtitle === 'Laptop mode · touch blocked') {
                phase++;
                GLib.spawn_command_line_async(`kill -USR1 ${GLib.getenv('TTT_MOCK_PID')}`);
            } else if (phase === 2 && tile.checked && tile.subtitle === 'Tablet mode · touch available') {
                phase++;
                tile.emit('clicked', 1);
            } else if (phase === 3 && !tile.checked && tile.subtitle === 'Off') {
                GLib.file_set_contents(`${dir}/passed`, 'ok');
                this._timer = 0;
                return GLib.SOURCE_REMOVE;
            }
            return GLib.SOURCE_CONTINUE;
        });
    }

    disable() {
        if (this._timer)
            GLib.Source.remove(this._timer);
    }
}
