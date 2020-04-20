# Build instruction

## Build environment

in petailinux > 19.1

	petalinxu-build --sdk

then

	petalinux-package --sysroot

## Build tcf agent

Source `petalinux` SDK environment (installed in ../image/linux/sdk directroy)

Update `CFLAGS` variable to fix some runtime issues

	export CFLAGS="$CFLAGS -fstack-protector-strong -Wformat -Wformat-security -Werror=format-security -D_FORTIFY_SOURCE=2 -DENABLE_HardwareBreakpoints=0"

In this directory issue command:

	make INSTALLROOT=/`pwd`/rfs/ OPSYS=GNU/Linux MACHINE=arm -C agent/
	#or
	make INSTALLROOT=/`pwd`/rfs/ OPSYS=GNU/Linux MACHINE=a64 -C agent/

To clean

	make INSTALLROOT=/`pwd`/rfs/ OPSYS=GNU/Linux MACHINE=arm -C agent/ clean
	#or
	make INSTALLROOT=/`pwd`/rfs/ OPSYS=GNU/Linux MACHINE=a64 -C agent/ clean

## Build deb package

	make OPSYS=GNU/Linux MACHINE=arm -C agent/ deb
	make OPSYS=GNU/Linux MACHINE=a64 -C agent/ deb

This will use `dpkg-deb --root-owner-group --build tcf-agent_1.7.0-r0_armhf` command. Make sure is installed and working



