Name:           gtkterm
Version:        2.0.0
Release:        1%{?dist}
Summary:        GTK4 serial port terminal emulator

License:        GPL-3.0-or-later
URL:            https://github.com/wvdakker/gtkterm
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(gtk4)
BuildRequires:  pkgconfig(vte-2.91-gtk4)
BuildRequires:  pkgconfig(gudev-1.0)

%description
GTKTerm is a simple graphical serial port terminal emulator built with GTK.

%prep
%autosetup -n %{name}-%{version}

%build
%meson
%meson_build

%install
%meson_install

%files
%license COPYING
%doc README.md NEWS
%{_bindir}/gtkterm
%{_datadir}/applications/org.gtk.gtkterm.desktop
%{_datadir}/icons/hicolor/*/apps/org.gtk.gtkterm.png
%{_datadir}/metainfo/org.gtk.gtkterm.appdata.xml
%{_datadir}/locale/*/LC_MESSAGES/gtkterm.mo

%changelog
* Tue Jun 17 2026 GTKTerm Maintainers <maintainers@example.com> - 2.0.0-1
- Initial RPM packaging for gtk4-port branch
