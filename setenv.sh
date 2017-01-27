##
## Small script to set the correct environment variables needed for building
## Minoca OS. Source this into your current environment by running:
## . ./setenv.sh
##

if [ "`basename -- $0`" = "setenv.sh" ]; then
    echo "Error: This script is meant to be sourced, not run. Use: '. $0' to \
source it into your current environment"
fi

if [ -z "$SRCROOT" ]; then
    cd ..
    export SRCROOT=$PWD
    cd - > /dev/null
fi

if [ -z "$ARCH" ]; then
    if [ "`uname -s`" = "Minoca" ]; then
        case "`uname -m`" in
        i586)  export ARCH=x86 VARIANT=q ;;
        i686)  export ARCH=x86 ;;
        armv6) export ARCH=armv6 ;;
        armv7) export ARCH=armv7 ;;
        *)     export ARCH=x86 ;;
        esac
    else
        export ARCH=x86
    fi
fi

if [ -z "$DEBUG" ]; then
    export DEBUG=dbg
fi

if ! echo "$PATH" | grep -q "$SRCROOT/$ARCH$VARIANT$DEBUG/tools/bin"; then
    OLDPATH="$PATH"
    PATH="$SRCROOT/$ARCH$VARIANT$DEBUG/tools/bin:$OLDPATH"
fi

##
## Perform some sanity checks on the environment.
##

if ! [ -d "$SRCROOT/os" ]; then
    echo "Warning: I don't see the os repository at $SRCROOT/os."
fi

case "$ARCH$VARIANT" in
    x86q)     target=i586-pc-minoca ;;
    x86)      target=i686-pc-minoca ;;
    armv[67]) target=arm-none-minoca ;;
    x64)      target=x86_64-none-minoca ;;
    *) echo "Warning: Unknown architecture $ARCH$VARIANT."
esac

tools="awk
$target-ld
$target-ar
$target-gcc
$target-g++
iasl
m4
make"

failed_tools=
for tool in $tools; do
    if ! which $tool > /dev/null 2>&1 && \
       ! which $tool.exe > /dev/null 2>&1; then

        failed_tools="$failed_tools $tool"
    fi
done

if [ "$failed_tools" ]; then
    if [ -d "$SRCROOT/third-party" ]; then
        echo "Warning: Failed to find tools$failed_tools. You may need to \
run 'make tools' in $SRCROOT/third-party."
    else
        echo "Warning: Failed to find tools$failed_tools. You may need to \
download the latest Minoca toolchain for your build OS, or rebuild the tools
from source by grabbing the third-party repository and running 'make tools'."
    fi
else
    echo "Minoca build environment for $ARCH$VARIANT."
fi

unset target tools failed_tools
