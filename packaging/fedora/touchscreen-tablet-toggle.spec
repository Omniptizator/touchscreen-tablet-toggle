Name:           touchscreen-tablet-toggle
Version:        0.1.1
Release:        1%{?dist}
Summary:        GNOME Quick Setting for tablet-only finger touch
License:        GPL-3.0-or-later
Source0:        %{name}-%{version}.tar.gz
BuildArch:      noarch

BuildRequires:  systemd-rpm-macros
BuildRequires:  python3-gobject-base python3-libevdev python3-pyudev
BuildRequires:  gjs dbus-daemon glib2
Requires:       python3-gobject-base python3-libevdev python3-pyudev
Requires:       polkit systemd
Requires:       gnome-shell >= 51
Requires:       gnome-shell < 52
%{?systemd_requires}

%description
Automatically block the internal finger touchscreen in laptop posture using
an Intel Virtual Switches sensor. Includes a system service and a GNOME
Shell extension; the separate pen and touchpad remain independent.

%prep
%setup -q

%build
# Interpreted Python and GJS.

%install
install -d %{buildroot}%{_prefix}/lib/%{name}
install -pm644 service/*.py %{buildroot}%{_prefix}/lib/%{name}/
ext=%{buildroot}%{_datadir}/gnome-shell/extensions/tablet-only-touchscreen@%{name}
install -d "$ext"
install -pm644 extension/* "$ext/"
install -Dpm644 debian/%{name}.service %{buildroot}%{_unitdir}/%{name}.service
install -Dpm644 data/*.conf -t %{buildroot}%{_datadir}/dbus-1/system.d
install -Dpm644 data/*.policy -t %{buildroot}%{_datadir}/polkit-1/actions

%check
GIO_USE_VFS=local PYTHONDONTWRITEBYTECODE=1 dbus-run-session -- \
    /usr/bin/python3 -m unittest discover -s tests -v

%post
%systemd_post %{name}.service

%preun
%systemd_preun %{name}.service

%postun
%systemd_postun_with_restart %{name}.service

%files
%license LICENSE
%doc README.md
%{_prefix}/lib/%{name}/
%{_unitdir}/%{name}.service
%{_datadir}/gnome-shell/extensions/tablet-only-touchscreen@%{name}/
%{_datadir}/dbus-1/system.d/io.github.TouchscreenTabletToggle.conf
%{_datadir}/polkit-1/actions/io.github.touchscreen-tablet-toggle.policy

%changelog
* Sun Nov 22 2026 Touchscreen Tablet Toggle contributors - 0.1.1-1
- Initial Fedora 45 package.
