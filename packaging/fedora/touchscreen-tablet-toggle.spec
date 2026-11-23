Name:           touchscreen-tablet-toggle
Version:        0.2.0
Release:        1%{?dist}
Summary:        GNOME Quick Setting for tablet-only finger touch
License:        GPL-3.0-or-later
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  systemd-rpm-macros
BuildRequires:  gcc make pkgconfig(gio-unix-2.0) pkgconfig(libevdev) pkgconfig(libudev)
BuildRequires:  gjs dbus-daemon glib2
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
%set_build_flags
%make_build

%install
install -Dpm755 build/%{name} %{buildroot}%{_libexecdir}/%{name}
ext=%{buildroot}%{_datadir}/gnome-shell/extensions/tablet-only-touchscreen@%{name}
install -d "$ext"
install -pm644 extension/* "$ext/"
install -Dpm644 debian/%{name}.service %{buildroot}%{_unitdir}/%{name}.service
install -Dpm644 data/*.conf -t %{buildroot}%{_datadir}/dbus-1/system.d
install -Dpm644 data/*.policy -t %{buildroot}%{_datadir}/polkit-1/actions

%check
make check

%post
%systemd_post %{name}.service

%preun
%systemd_preun %{name}.service

%postun
%systemd_postun_with_restart %{name}.service

%files
%license LICENSE
%doc README.md
%{_libexecdir}/%{name}
%{_unitdir}/%{name}.service
%{_datadir}/gnome-shell/extensions/tablet-only-touchscreen@%{name}/
%{_datadir}/dbus-1/system.d/io.github.TouchscreenTabletToggle.conf
%{_datadir}/polkit-1/actions/io.github.touchscreen-tablet-toggle.policy

%changelog
* Sun Nov 22 2026 Touchscreen Tablet Toggle contributors - 0.1.1-1
- Initial Fedora 45 package.
