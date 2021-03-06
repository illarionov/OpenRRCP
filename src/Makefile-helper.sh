#!/bin/sh

sys=`uname -s`

if test "$sys" = "Linux"; then
    echo -n "-s -Wall -O2"
    exit;
fi;

if test "$sys" = "NetBSD"; then
    echo -n "-s -Wall -O2 -lpcap -I/usr/pkg/include -L/usr/pkg/lib"
    exit;
fi;

if test "$sys" = "FreeBSD"; then
    echo -n "-s -Wall -O2 -lpcap -L/usr/lib"
    exit;
fi;

if test "$sys" = "OpenBSD"; then
    echo -n "-s -Wall -O2 -lpcap -I/usr/local/include -L/usr/local/lib"
    exit;
fi;

if test "$sys" = "Darwin"; then
    echo -n "-s -Wall -O2 -ldnet -lpcap -I/opt/local/include -L/opt/local/lib -L/usr/lib"
    exit;
fi;

#default
echo -n "-s -Wall -O2"
exit;
