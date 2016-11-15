#! /usr/bin/python
## Copyright (c) 2015 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     build_image.py
##
## Abstract:
##
##     This script builds a custom Minoca OS image.
##
## Author:
##
##     Evan Green 27-Feb-2015
##
## Environment:
##
##     Build (Python)
##

import base64
import json
import os
import shutil
import stat
import sys

try:
    from shlex import quote

except ImportError:
    from pipes import quote

##
## Define binaries and libraries to always put in.
##

SYSTEM_BIN = [
    'debug',
    'efiboot',
    'mount',
    'umount',
    'msetup',
    'profile',
    'swiss',
    'vmstat',
]

SYSTEM_LIB = [
    'libc.so.1',
    'libminocaos.so.1',
    'libcrypt.so.1',
]

def run(working_dir, command):
    os.chdir(working_dir)
    status = os.system(command)
    if (not os.WIFEXITED(status)) or (os.WEXITSTATUS(status) != 0):
        print("Command %s returned with status %x" % (command, status))
        exit(1)

    return

def main(argv):
    recipe_file = open("build_request.json", "r")
    recipe = json.loads(recipe_file.read())
    recipe_file.close()
    cwd = os.getcwd()
    imagescriptdir = os.path.join(cwd, "..", "..", "tasks", "images")
    platform = recipe['platform']
    if platform == 'PC':
        arch='x86'
        image_script = os.path.join(imagescriptdir, "build_pc.sh")
        image = "pc.img"

    elif platform == 'UEFI-based PC':
        arch='x86'
        image_script = os.path.join(imagescriptdir, "build_pcefi.sh")
        image = "pcefi.img"

    elif platform == 'Raspberry Pi':
        arch='armv6'
        image_script = os.path.join(imagescriptdir, "build_rpi.sh")
        image = "rpi.img"

    elif platform == 'TI PandaBoard':
        arch='armv7'
        image_script = os.path.join(imagescriptdir, "build_panda.sh")
        image = "panda.img"

    elif platform == 'Intel Galileo':
        arch='x86q'
        image_script = os.path.join(imagescriptdir, "build_pcefi.sh")
        image = "pcefi.img"

    else:
        print("Error: Unknown platform %s\n" % platform)
        return 1

    pkgroot = os.path.normpath(os.path.join(cwd, "..", arch, "packages"))
    binroot = os.path.normpath(os.path.join(cwd, "..", arch, "bin"))
    appsdir = os.path.join(binroot, "apps")

    ##
    ## Remove the old apps directory.
    ##

    shutil.rmtree(appsdir, True)
    os.mkdir(appsdir)

    ##
    ## Copy in all the binaries and libraries that should be there.
    ##

    bindir = os.path.join(appsdir, "bin")
    os.mkdir(bindir)
    for binitem in SYSTEM_BIN:
        dest = os.path.join(bindir, binitem)
        source = os.path.join(".", binitem)
        command = "cp -pv -- '%s' '%s'" % (source, dest)
        run(binroot, command)

    libdir = os.path.join(appsdir, "lib")
    os.mkdir(libdir)
    for libitem in SYSTEM_LIB:
        dest = os.path.join(libdir, libitem)
        source = os.path.join(".", libitem)
        command = "cp -pv -- '%s' '%s'" % (source, dest)
        run(binroot, command)

    os.makedirs(os.path.join(appsdir, "usr", "lib", "opkg"))

    ##
    ## Copy the skeleton directory over.
    ##

    run(binroot, "cp -Rpv ./skel/* ./apps/")

    ##
    ## Symlink the core utilities.
    ##

    command = "for app in `/bin/swiss --list`; do ln -s swiss $app; done"
    run(bindir, command)

    ##
    ## Update the list of available packages.
    ##

    print("Updating package list.")
    opkg_conf = os.path.join(cwd, "..", arch, "opkg.conf")
    opkg_args = "--offline-root='%s' --conf='%s' -V2 " % (appsdir, opkg_conf)
    run(appsdir, "opkg %s update" % opkg_args)

    ##
    ## Install all packages.
    ##

    for package in recipe['package_list']:
        print("Installing package %s" % package)
        run(appsdir,
            "opkg %s install --force-postinstall %s" % (opkg_args, package))

    ##
    ## Add the users and groups.
    ##

    next_id = 1000
    for user in recipe['user_list']:
        name = quote(user['username'])
        gecos = quote(base64.b64decode(user['gecos']))
        enable_password = user['enable_password']
        password = quote(base64.b64decode(user['password']))
        extra_args = ''
        id = next_id
        if name == 'root':
            command = 'userdel --root="%s" root' % appsdir
            run(appsdir, command)
            id = 0
            extra_args = '--home=/root'

        command = 'useradd --root="%s" --comment=%s -mU %s -u%d' % \
                  (appsdir, gecos, extra_args, id)

        if enable_password:
            command += ' --password=%s ' % password

        command += ' %s ' % name
        run(appsdir, command)
        if id == next_id:
            next_id += 1

        ##
        ## Add the authorized keys if supplied.
        ##

        authorized_keys = base64.b64decode(user['ssh_authorized_keys'])
        if len(authorized_keys):
            dotssh = os.path.join(appsdir, 'home', name, '.ssh')
            keyspath = os.path.join(dotssh, 'authorized_keys')
            os.mkdir(dotssh)
            f = open(keyspath, "w")
            f.write(authorized_keys)
            f.close()
            os.chmod(keyspath, stat.S_IRUSR | stat.S_IWUSR)
            os.chown(keyspath, id, id)
            mode = stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR | \
                   stat.S_IRGRP | stat.S_IWGRP | stat.S_IXGRP | \
                   stat.S_IROTH | stat.S_IXOTH

            os.chmod(dotssh, mode)
            os.chown(dotssh, id, id)

    ##
    ## Handle the image size parameter.
    ##

    os.environ['CI_MIN_IMAGE_SIZE'] = str(recipe['image_size'])

    ##
    ## Create the image.
    ##

    print("Creating final image.")
    if arch == 'x86q':
        os.environ['ARCH'] = 'x86'
        os.environ['VARIANT'] = 'q'

    else:
        os.environ['ARCH'] = arch

    os.environ['DEBUG'] = 'dbg'
    run(binroot, "sh '%s'" % image_script)

    ##
    ## Zip the image.
    ##

    print("Archiving image.")
    command = "mv '%s' minocaos.img" % image
    run(binroot, command)
    command = "tar -czf minocaos.tar.gz ./minocaos.img"
    run(binroot, command)

    ##
    ## Upload the image.
    ##

    print("Uploading image.")
    command = "python ../../../client.py --upload schedule " \
              "minocaos.tar.gz minocaos.tar.gz"

    run(binroot, command)
    print("Successfully created OS image.")
    return 0

if __name__ == '__main__':
    exit(main(sys.argv))

