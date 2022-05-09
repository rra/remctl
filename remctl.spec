# RPM spec file for remctl.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Improvements by Thomas Kula and Darren Patterson
# Copyright 2018, 2020, 2022 Russ Allbery <eagle@eyrie.org>
# Copyright 2006-2007, 2012-2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# SPDX-License-Identifier: MIT

%if 0%{?sles_version:1}
%define relsuffix sles%{sles_version}
%if %{sles_version} >= 12
%define with_systemd 1
%endif
%else
%define rel %(cat /etc/redhat-release | cut -d ' ' -f 7 | cut -d'.' -f1)
%define relsuffix EL%{rel}
%if %{rel} >= 7
%define with_systemd 1
%endif
%endif

# this is needed for Stanford packaging automation
%define vers 3.18

# Use rpmbuild option "--define 'buildperl 0'" to not build the Perl module.
%{!?buildperl:%define buildperl 1}
# Use rpmbuild option "--define 'buildperl 0'" to not build the php bindings.
%{!?buildphp:%define buildphp 1}
# Use rpmbuild option "--define 'buildperl 0'" to not build the ruby bindings.
%{!?buildruby:%define buildruby 1}

# Use rpmbuild option "--define 'buildpython 0'" to not build the Python module.
%{!?buildpython:%define buildpython 1}
%if %{buildpython}
%define py_version %(python -c "from distutils.sysconfig import get_python_version; print(get_python_version())" )
%define py_libdest %(python -c "from distutils.sysconfig import get_config_vars; print(get_config_vars()[ 'LIBDEST' ])")
%define py_binlibdest %(python -c "from distutils.sysconfig import get_config_vars; print(get_config_vars()[ 'BINLIBDEST' ])")
%endif

Name: remctl
Summary: Client/server for Kerberos-authenticated command execution
Version: %{vers}
Release: 1.%{relsuffix}
%if 0%{?rel} >= 4 || 0%{?sles_version:1}
License: MIT
%else
Copyright: MIT
%endif
URL: https://www.eyrie.org/~eagle/software/remctl/
Source: https://archives.eyrie.org/software/kerberos/%{name}-%{version}.tar.gz
Group: System Environment/Daemons
Vendor: Stanford University
Packager: Russ Allbery <eagle@eyrie.org>
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: krb5-devel, libgcrypt, libevent-devel
%if %{buildperl}
BuildRequires: perl(Module::Build)
%endif
%if %{buildpython}
BuildRequires: python-devel, python
%endif
%if %{buildphp}
BuildRequires: php-devel
%endif
%if %{buildruby}
BuildRequires: ruby, ruby-devel
%endif
%if 0%{?sles_version:1}
%if 0%{?with_systemd:1}
BuildRequires: systemd-rpm-macros
%endif
Distribution: SUSE Linux Enterprise %{sles_version}
%else
%if 0%{?with_systemd:1}
BuildRequires: systemd-units
%endif
Distribution: EL
%endif

%ifarch i386
BuildArch: i686
%endif

%if %{buildphp}
# RHEL 5/6 compatibility for PHP
%if 0%{?rel} == 5
%global php_apiver %((echo 0; php -i 2>/dev/null | sed -n 's/^PHP API => //p') | tail -1)
%{!?php_extdir: %{expand: %%global php_extdir %(php-config --extension-dir)}}
%endif
%if 0%{?rel} == 5 || 0%{?rel} == 6
%{!?php_inidir: %{expand: %%global php_inidir %{_sysconfdir}/php.d }}
%endif
%endif

%if %{buildpython}
# RHEL 5 compatibility for Python
%if 0%{?rel} == 5
%{!?python_sitelib: %global python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")}
%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(1))")}
%endif
%endif

%if %{buildruby}
# RHEL 5/6 compatibility for Ruby
%if 0%{?rel} == 5
%{!?ruby_vendorarchdir: %global ruby_vendorarchdir %(ruby -rrbconfig -e 'puts Config::CONFIG["sitearchdir"] ')}
%endif
%if 0%{?rel} == 6
%{!?ruby_vendorarchdir: %global ruby_vendorarchdir %(ruby -rrbconfig -e 'puts Config::CONFIG["vendorarchdir"] ')}
%endif
%endif


%description
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

%package server
Summary: Server for Kerberos-authenticated command execution
Group: System Environment/Daemons
%{?systemd_requires}

%description server
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

This package contains the server (remctld).

%package devel
Summary: Development files for remctl
Group: Applications/Internet
Requires: %{name}-client = %{version}-%{release}

%description devel
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

This package contains the development files.

%package client
Summary: Client for Kerberos-authenticated command execution
Group: Applications/Internet

%description client
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

This package contains the client program (remctl) and the client libraries.

%if %{buildphp}
%package php
Summary: PHP interface to remctl
Group: Development/Libraries
Requires: %{name}-client = %{version}-%{release}
%if 0%{?rel} == 5
Requires:     php-api = %{php_apiver}
%else
Requires:     php(zend-abi) = %{php_zend_api}
Requires:     php(api) = %{php_core_api}
%endif

%description php
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

This package contains the PHP remctl client library.
%endif

%if %{buildpython}
%package python
Summary: Python library for Kerberos-authenticated command execution
Group: Applications/Internet
Requires: %{name}-client = %{version}-%{release}

%description python
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

This package contains the Python remctl client library.
%endif

%if %{buildruby}
%package ruby
Summary: Ruby interface to remctl
Group: Development/Libraries
Requires: %{name}-client = %{version}-%{release}
%if 0%{?rel} <= 6
Requires: ruby(abi) = 1.8
%else
Requires: ruby(abi) = 1.9.1
%endif
Provides: ruby(remctl) = %{version}-%{release}

%description ruby
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

This package contains the Ruby remctl client library.
%endif

%if %{buildperl}
%package perl
Summary: Perl library for Kerberos-authenticated command execution
Group: Applications/Internet
%if 0%{?sles_version:1} == 0
Requires: perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))
%endif
Requires: %{name}-client = %{version}-%{release}

%description perl
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

This package contains the Perl remctl client library.
%endif

%prep
%setup -q -n remctl-%{version}

%build
options="--disable-static --libdir=%{_libdir} --sysconfdir=/etc/remctl"
%if %{buildruby}
options="$options --enable-ruby"
%endif
%if %{buildphp}
options="$options --enable-php"
%endif
%if %{buildperl}
options="$options --enable-perl"
%endif
%if %{buildpython}
options="$options --enable-python"
%endif
%if %{buildperl}
export PATH="/usr/kerberos/bin:/sbin:/bin:/usr/sbin:$PATH"
export REMCTL_PERL_FLAGS="--installdirs=vendor"
%if 0%{?rel} >= 6
export REMCTL_PERL_FLAGS="$REMCTL_PERL_FLAGS --prefix=/usr"
%endif
%endif
%configure $options
%{__make}

%install
%{__rm} -rf %{buildroot}
options=''
%if %{buildruby}
options="$options RUBYARCHDIR=%{buildroot}%{ruby_vendorarchdir}"
%endif
make install DESTDIR=%{buildroot} INSTALL="install -p" \
    INSTALLDIRS=vendor $options
%if !0%{?with_systemd:1}
mkdir -p %{buildroot}/etc/xinetd.d
install -c -m 0644 examples/xinetd %{buildroot}/etc/xinetd.d/remctl
%endif
mkdir -p %{buildroot}/etc/remctl/acl
mkdir -p %{buildroot}/etc/remctl/conf.d
mkdir -p %{buildroot}/usr/share/doc/remctl-{server,client}-%{vers}
chmod 755 %{buildroot}/usr/share/doc/remctl-{server,client}-%{vers}
install -c -m 0644 examples/remctl.conf %{buildroot}/etc/remctl/remctl.conf
%if %{buildperl}
find %{buildroot} -type f -name perllocal.pod -exec rm -f {} \;
find %{buildroot} -type f -name .packlist -exec rm -f {} \;
find %{buildroot} -type f -name '*.bs' -size 0 -exec rm -f {} \;
find %{buildroot} -type f -name Remctl.so -exec chmod 755 {} \;
mkdir -p %{buildroot}/usr/share/doc/remctl-perl-%{vers}
chmod 755 %{buildroot}/usr/share/doc/remctl-perl-%{vers}
%endif
%if %{buildpython}
mkdir -p %{buildroot}/usr/share/doc/remctl-python-%{vers}
chmod 755 %{buildroot}/usr/share/doc/remctl-python-%{vers}
find %{buildroot} -name _remctl.so -exec chmod 755 {} \;
%endif
%if %{buildruby}
mkdir -p %{buildroot}/usr/share/doc/remctl-ruby-%{vers}
chmod 755 %{buildroot}/usr/share/doc/remctl-ruby-%{vers}
%endif
%if %{buildphp}
mkdir -p %{buildroot}/usr/share/doc/remctl-php-%{vers}
chmod 755 %{buildroot}/usr/share/doc/remctl-php-%{vers}
# PHP configuration
mkdir -p %{buildroot}%{php_inidir}
install -m 0644 -p php/remctl.ini %{buildroot}%{php_inidir}
%endif
%if 0%{?sles_version:1}
mkdir -p %{buildroot}/etc/sysconfig/SuSEfirewall2.d/services
cat <<EOF >%{buildroot}/etc/sysconfig/SuSEfirewall2.d/services/remctld
## Name: Remctl Server
## Description: Open ports for Kerberos-authenticated command execution

TCP="remctl"
EOF
%endif

%files devel
%defattr(-, root, root)
/usr/include/remctl.h
%doc %{_mandir}/man3/remctl*
%{_libdir}/pkgconfig/libremctl.pc

%files client
%defattr(-, root, root)
%{_bindir}/*
%doc NEWS README TODO
%{_libdir}/libremctl.la
%{_libdir}/libremctl.so
%{_libdir}/libremctl.so.*
%doc %{_mandir}/man1/remctl.*

%files server
%defattr(-, root, root)
%dir /etc/remctl
%{_sbindir}/*
%doc NEWS README TODO
%doc %{_mandir}/*/remctld.*
%if !0%{?with_systemd:1}
%config /etc/xinetd.d/remctl
%endif
%config(noreplace) /etc/remctl/remctl.conf
%dir /etc/remctl/acl/
%dir /etc/remctl/conf.d/
%if 0%{?sles_version:1}
/etc/sysconfig/SuSEfirewall2.d/services/remctld
%endif
%if 0%{?with_systemd:1}
%{_unitdir}/remctld.service
%{_unitdir}/remctld.socket
%endif

%if %{buildpython}
%files python
%defattr(-, root, root)
%{python_sitearch}/_remctl.so
%{python_sitearch}/remctl.py*
%if 0%{?rel} != 5
%{python_sitearch}/pyremctl-%{version}-*.egg-info
%endif
%doc NEWS TODO
%doc python/README
%endif

%if %{buildperl}
%files perl
%defattr(-,root,root,-)
%{perl_vendorarch}/Net
%{perl_vendorarch}/auto/Net
%doc %{_mandir}/man3/Net::Remctl*
%doc NEWS TODO
%endif

%if %{buildphp}
%files php
%defattr(-,root,root,-)
%doc NEWS TODO
%doc php/README
%{php_extdir}/remctl.so
%config(noreplace) %{php_inidir}/remctl.ini
%endif

%if %{buildruby}
%defattr(-,root,root,-)
%files ruby
%defattr(-,root,root,-)
%doc NEWS TODO
%doc ruby/README
%{ruby_vendorarchdir}/remctl.so
%endif

%if 0%{?with_systemd:1} && 0%{?rel:1}
%preun server
%systemd_preun remctld.service
%endif
%if 0%{?with_systemd:1} && 0%{?sles_version:1}
%pre server
%service_add_pre remctld.service
%preun server
%service_del_preun remctld.service
%endif

%post client -p /sbin/ldconfig

%post server 
# If this is the first remctl install, add remctl to /etc/services and
# restart xinetd to pick up its new configuration.
if [ "$1" = 1 ] ; then
    if grep -q '^remctl' /etc/services ; then
        :
    else
        echo 'remctl    4373/tcp' >> /etc/services
    fi
%if !0%{?with_systemd:1}
    if [ -f /var/run/xinetd.pid ] ; then
        kill -HUP `cat /var/run/xinetd.pid`
    fi
%endif
fi
%if 0%{?with_systemd:1} && 0%{?rel:1}
%systemd_post remctld.service
%endif
%if 0%{?with_systemd:1} && 0%{?sles_version:1}
%service_add_post remctld.service
%endif

%postun client -p /sbin/ldconfig

%postun server
# If we're the last remctl package, remove the /etc/services line and
# restart xinetd to reflect its new configuration.
if [ "$1" = 0 ] ; then
    if [ ! -f /usr/sbin/remctld ] ; then
        if grep -q "^remctl" /etc/services ; then
            grep -v "^remctl" /etc/services > /etc/services.tmp
            mv -f /etc/services.tmp /etc/services
        fi
%if !0%{?with_systemd:1}
        if [ -f /var/run/xinetd.pid ] ; then
            kill -HUP `cat /var/run/xinetd.pid`
        fi
%endif
    fi
fi
%if 0%{?with_systemd:1} && 0%{?rel:1}
%systemd_postun_with_restart remctld.service
%endif
%if 0%{?with_systemd:1} && 0%{?sles_version:1}
%service_del_postun remctld.service
%endif


%clean
%{__rm} -rf %{buildroot}

%changelog
* Sun May 8 2022 Russ Allbery <eagle@eyrie.org> 3.18-1
- update to 3.18

* Sun Dec 13 2020 Russ Allbery <eagle@eyrie.org> 3.17-1
- update to 3.17

* Sat May 5 2018 Russ Allbery <eagle@eyrie.org> 3.15-1
- update to 3.15

* Sat Mar 31 2018 Russ Allbery <eagle@eyrie.org> 3.14-1
- update to 3.14

* Sat May 7 2016 Russ Allbery <eagle@eyrie.org> 3.11-1
- update to 3.11
- add systemd support
- support SLES
- add libevent dependency

* Thu Apr 11 2013 Darren Patterson <darrenp1@stanford.edu> 3.4-1
- update to 3.4, and merged some changes from ktdreyer in EPEL spec
- split out perl, php, ruby

* Tue Jul 17 2012 Darren Patterson <darrenp1@stanford.edu> 3.2-2
- spec fixes for build issues around perl and lib dirs
- move perl to vendor_perl on EL6
- re-add missing devel package
- change arch to i686 from i386 (if 32bit build)
- added missing/required doc to python package
- general cleanup for rpmlint

* Tue Jun 19 2012 Russ Allbery <eagle@eyrie.org> 3.2-1
* Update for 3.2.

* Thu Feb 23 2012 Russ Allbery <eagle@eyrie.org> 3.1-1
- Update for 3.1.

* Wed Feb 15 2012 Thomas L. Kula <tlk2126@columbia.edu> 3.0-1
- Update for 3.0
- Add support for Python bindings, building as a sub-package

* Sun May 2 2010 Russ Allbery <eagle@eyrie.org> 2.16-1
- Update for 2.16.
- Ruby bindings also not yet supported.

* Fri Nov 14 2008 Russ Allbery <eagle@eyrie.org> 2.13-1
- Update for 2.13.
- PHP and Python bindings not yet supported.

* Fri Apr 4 2008 Russ Allbery <eagle@eyrie.org> 2.12-1
- Update for 2.12.

* Fri Nov 9 2007 Russ Allbery <eagle@eyrie.org> 2.11-1
- Update for 2.11.
- Change port configuration to 4373.

* Sun Aug 26 2007 Russ Allbery <eagle@eyrie.org> 2.10-1
- Update for 2.10.
- Incorporate changes by Darren Patterson to install the Perl module.

* Sun Mar 25 2007 Russ Allbery <eagle@eyrie.org> 2.7-1
- Update for 2.7.

* Sat Feb 3 2007 Russ Allbery <eagle@eyrie.org> 2.6-1
- Update for 2.6.

* Sat Feb 3 2007 Russ Allbery <eagle@eyrie.org> 2.5-1
- Update for 2.5.
- Incorporate changes by Darren Patterson to split into subpackages for
  client and server and remove krb5-workstation requirement.

* Wed Jan 17 2007 Russ Allbery <eagle@eyrie.org> 2.4-1
- Update for 2.4.
- Changed permissions on the ACL directory to allow any user read.
- Added fix for /usr/lib64 directory on x86_64.

* Tue Aug 22 2006 Russ Allbery <eagle@eyrie.org> 2.1-1
- Update for 2.1.
- Incorporate changes by Digant Kasundra and Darren Patterson to the
  Stanford-local RPM spec file, including looking for libraries in the
  correct path, creating the /etc/remctl structure, running ldconfig,
  and handling library installation properly.
- Build remctl to look in /etc/remctl/remctl.conf for its config.
- Install the examples/remctl.conf file as /etc/remctl/remctl.conf.

* Mon Aug  7 2006 Russ Allbery <eagle@eyrie.org> 2.0-1
- New upstream release that now provides a library as well.
- Add TODO to documentation files.

* Fri Mar 24 2006 Russ Allbery <eagle@eyrie.org> 1.12-3
- Lots of modifications based on the contributed kstart spec file.
- Use more internal RPM variables.
- Borrow the description from the Debian packages.

* Sun Feb 19 2006 Digant C Kasundra <digant@stanford.edu> 1.12-2
- Initial version.
