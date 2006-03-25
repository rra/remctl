Name: remctl
Summary: Client/server for Kerberos-authenticated command execution
Version: 1.12
Release: 3
Copyright: MIT
URL: http://www.eyrie.org/~eagle/software/remctl/
Source: http://archives.eyrie.org/software/kerberos/%{name}-%{version}.tar.gz
Group: Server/Monitoring
Vendor: Stanford University
Packager: Digant C Kasundra <digant@stanford.edu>
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: krb5-devel
Requires: krb5-workstation
Distribution: EL
ExclusiveArch: i686

%description
remctl is a client/server protocol for executing specific commands on a
remote system with Kerberos authentication.  The allowable commands must
be listed in a server configuration file, and the executable run on the
server may be mapped to any command name.  Each command is also associated
with an ACL containing a list of Kerberos principals authorized to run
that command.

%prep
%setup

%build
PATH="/sbin:/bin:/usr/sbin:$PATH \
%configure
%{__make}

%install
%{__rm} -rf %{buildroot}
%makeinstall
mkdir -p %{buildroot}/etc/xinetd.d
install -c -m 0644 examples/xinetd %{buildroot}/etc/xinetd.d/remctl

%files
%defattr(-, root, root, 0755)
%doc NEWS README
%{_bindir}/*
%{_sbindir}/*
%{_mandir)/*/*

%post
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
# If we're the last remctl package, remove the /etc/services line and
# restart xinetd to reflect its new configuration.
if [ "$1" = 0 ] ; then
    # This check will be necessary with client and server are split into
    # two packages.
    if [ ! -f /usr/local/sbin/remctld ] ; then
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
* Fri Mar 24 2006 Russ Allbery <rra@stanford.edu> 1.12-3
- Lots of modifications based on the contributed kstart spec file.
- Use more internal RPM variables.
- Borrow the description from the Debian packages.

* Sun Feb 19 2006 Digant C Kasundra <digant@stanford.edu> 1.12-2
- Initial version.
