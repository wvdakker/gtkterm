#!/bin/sh
# Run this to generate all the initial makefiles, etc.

topdir=`dirname $0`
test -z "$topdir" && topdir=.

PKG_NAME="GtkTerm"

(test -f $topdir/src/gtkterm.c) || {
    echo -n "**Error**: Directory "\`$topdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

# Check for gnome-autogen.sh existance
which gnome-autogen.sh || {
    echo "You need to install gnome-common from GNOME Git (or from"
    echo "your OS vendor's package manager)."
    exit 1
}

USE_GNOME3_MACROS=1 . gnome-autogen.sh
