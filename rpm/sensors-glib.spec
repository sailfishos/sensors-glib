Name:             sensors-glib
Summary:          Sailfish sensors API for glib based C applications
Version:          1.0.0
Release:          0
License:          BSD
URL:              https://github.com/sailfishos/sensors-glib/
Source0:          %{name}-%{version}.tar.bz2
Requires:         sensorfw-qt5 >= 0.14.3
Requires(post):   /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:    pkgconfig(glib-2.0)
BuildRequires:    pkgconfig(gio-2.0)

%description
This package contains C language client library for communicating with
Sailfish OS sensor daemon implemented as a collection of glib objects.

%package devel
Summary:    Development files for sensors-glib
Requires:   %{name} = %{version}-%{release}

%description devel
This package contains headers and link libraries needed for developing
programs that want to communicate with the Sailfish OS sensor daemon.

%prep
%setup -q -n %{name}-%{version}

%build
util/verify_version.sh
unset LD_AS_NEEDED
make _LIBDIR=%{_libdir} VERSION=%{version} %{_smp_mflags}

%install
make _LIBDIR=%{_libdir} VERSION=%{version} DESTDIR=%{buildroot} install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%license LICENSE.BSD
%{_libdir}/lib*.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/lib*.so
%dir %{_includedir}/sensors-glib
%{_includedir}/sensors-glib/*.h
%dir %{_libdir}/pkgconfig
%{_libdir}/pkgconfig/*.pc
