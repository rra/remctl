%define rel %(cat /etc/redhat-release | cut -d' ' -f7)

Name: remctl
Summary: Client/server for Kerberos-authenticated command execution
Version: 2.1
Release: 1.EL%{rel}
Copyright: MIT
URL: http://www.eyrie.org/~eagle/software/remctl/
Source: http://archives.eyrie.org/software/kerberos/%{name}-%{version}.tar.gz
Group: System Environment/Daemons
Vendor: Stanford University
Packager: Russ Allbery <rra@stanford.edu>
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: krb5-devel
Requires: krb5-workstation
Distribution: EL

%ifarch x86_64
%define ldir /usr/lib64
%else
%define ldir /usr/lib
%endif

%description
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

%prep
%setup -n remctl-%{version}

%build
PATH="/sbin:/bin:/usr/sbin:$PATH" \
%configure --prefix=/usr --mandir=/usr/share/man --sysconfdir=/etc/remctl
%{__make}

%install
%{__rm} -rf %{buildroot}
%makeinstall
mkdir -p %{buildroot}/etc/xinetd.d
install -c -m 0644 examples/xinetd %{buildroot}/etc/xinetd.d/remctl
mkdir -p %{buildroot}/etc/remctl/acl
mkdir -p %{buildroot}/etc/remctl/conf.d
install -c -m 0644 examples/remctl.conf %{buildroot}/etc/remctl/remctl.conf

%files
%defattr(-, root, root, 0755)
%doc NEWS README TODO
%{_bindir}/*
%{_sbindir}/*
%{_mandir}/*/*
/etc/xinetd.d/remctl
/usr/include/remctl.h
%dir /etc/remctl/
%defattr(0640, root, root)
%config(noreplace) /etc/remctl/remctl.conf
%defattr(0750, root, root)
%dir /etc/remctl/acl/
%dir /etc/remctl/conf.d/
%defattr(0644, root, root)
%{ldir}/libremctl.a
%{ldir}/libremctl.la
%{ldir}/libremctl.so.1.0.0

%post
/sbin/ldconfig

# If this is the first remctl install, add remctl to /etc/services and
# restart xinetd to pick up its new configuration.
if [ "$1" = 1 ] ; then
    if grep -q '^remctl' /etc/services ; then
        :
    else
        echo 'remctl    4444/tcp' >> /etc/services
    fi
    if [ -f /var/run/xinetd.pid ] ; then
        kill -HUP `cat /var/run/xinetd.pid`
    fi
fi

%postun
/sbin/ldconfig

# If we're the last remctl package, remove the /etc/services line and
# restart xinetd to reflect its new configuration.
if [ "$1" = 0 ] ; then
    # This check will be necessary with client and server are split into
    # two packages.
    if [ ! -f /usr/sbin/remctld ] ; then
        if grep -q "^remctl" /etc/services ; then
            grep -v "^remctl" /etc/services > /etc/services.tmp
            mv -f /etc/services.tmp /etc/services
        fi
        if [ -f /var/run/xinetd.pid ] ; then
            kill -HUP `cat /var/run/xinetd.pid`
        fi
    fi
fi

%clean
%{__rm} -rf %{buildroot}

%changelog
* Tue Aug 22 2006 Russ Allbery <rra@stanford.edu> 2.1-1
- Update for 2.1.
- Incorporate changes by Digant Kasundra and Darren Patterson to the
  Stanford-local RPM spec file, including looking for libraries in the
  correct path, creating the /etc/remctl structure, running ldconfig,
  and handling library installation properly.
- Build remctl to look in /etc/remctl/remctl.conf for its config.
- Install the examples/remctl.conf file as /etc/remctl/remctl.conf.

* Mon Aug  7 2006 Russ Allbery <rra@stanford.edu> 2.0-1
- New upstream release that now provides a library as well.
- Add TODO to documentation files.

* Fri Mar 24 2006 Russ Allbery <rra@stanford.edu> 1.12-3
- Lots of modifications based on the contributed kstart spec file.
- Use more internal RPM variables.
- Borrow the description from the Debian packages.

* Sun Feb 19 2006 Digant C Kasundra <digant@stanford.edu> 1.12-2
- Initial version.
