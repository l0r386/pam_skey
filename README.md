[![Travis CI Build Status](https://travis-ci.org/dkorunic/pam_skey.svg?branch=master)](https://travis-ci.org/dkorunic/pam_skey)

About
-----
This is complete pam_skey modul as interface to existing S/Key
library/interface by Wietse Venema. So far it has been tested on Linux
(Debian potato, Debian woody and Debian sid), Solaris 2.6, 2.7 and 2.8. I
have reason to believe it will work where PAM implementation exists and is
properly done. It provides **auth** services only. It can use tokens from
PAM_AUTHTOK to implement flexibile module stacking.

NOTE: You **absolutely** need libskey to make this module working. If there
is no libskey available on your system, configure will flag built-in skey
library (from logdaemon-5.10 package) for use.

This suite consists of following modules:

* pam_skey.so

  Main module than by default contains routines for checking skey.access
  for access rules

* pam_skey_access.so
  
  Add-on module that is useful only when it is required (requisite) module
  after pam_skey, which means skey.access controls future loading of other
  modules. Note that this emulates behavior of original Venema's
  skeylogin.

Known problems
--------------
Note that there are several problems with existing (original) Wietse
Venema's implementation - no flock() on keys file, hairy host checking
code in skeyaccess(), etc. If there is any other library function and
parametric compatible with S/Key library, this will most probably work
with it, too. I am planning to re-implement S/Key library later, having
Venema's and Linux-skey package as main resources.

Known bugs
----------
None reported so far. That does not mean there are none: if you find any
bug, or something that is likely to be bug, try to exactly see what
happens using `debug' option in local pam configuration file and e-mail me
together with console and syslog information. Or at least e-mail me with
feedback that can help to trace problems.

Installation
------------
Please please please, for installation instructions look closely at
INSTALL file.

Literature and other sources
----------------------------
I find most helpful was Linux-PAM documentation. I had also a peek into
following sources:
* Wietse Venema's logdaemon package
* Olaf Kirch's Linux S/Key package
* Wyman Miles' pam_securid module
* Linux-PAM modules and templates

Contact
-------
My e-mail address is Dinko Korunic <dinko.korunic@gmail.com> and PGP key
[0xEA160D0B](http://pgp.mit.edu/pks/lookup?op=get&search=0x6D3BAED1EA160D0B).
