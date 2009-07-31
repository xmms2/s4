# encoding: utf-8
#
# WAF build scripts for XMMS2
# Copyright (C) 2006-2009 XMMS2 Team
#

import sys
if sys.version_info < (2,4):
    raise RuntimeError("Python 2.4 or newer is required")

import os
import optparse

# Waf removes the current dir from the python path. We readd it to
sys.path.insert(0,os.getcwd())

import Options
import Utils
import Build
import Configure
from logging import fatal, warning

BASEVERSION="0.01"
APPNAME='s4'

srcdir='.'
blddir = '_build_'

####
## Initialization
####
def init():
    import gc
    gc.disable()


####
## Build
####
def build(bld):
    subdirs = bld.env["S4_SUBDIRS"]
    newest = max([os.stat(os.path.join(sd, "wscript")).st_mtime for sd in subdirs])
    if bld.env['NEWEST_WSCRIPT_SUBDIR'] and newest > bld.env['NEWEST_WSCRIPT_SUBDIR']:
        fatal("You need to run waf configure")
        raise SystemExit

    bld.add_subdirs(subdirs)


####
## Configuration
####
def configure(conf):
    if os.environ.has_key('PKG_CONFIG_PREFIX'):
        prefix = os.environ['PKG_CONFIG_PREFIX']
        if not os.path.isabs(prefix):
            prefix = os.path.abspath(prefix)
        conf.env['PKG_CONFIG_DEFINES'] = {'prefix':prefix}

    if Options.options.manualdir:
        conf.env["MANDIR"] = Options.options.manualdir
    else:
        conf.env["MANDIR"] = os.path.join(conf.env["PREFIX"], "share", "man")

    conf.env["S4_SUBDIRS"] = ["src"];

    if Options.options.build_tests:
        conf.env.append_value("S4_SUBDIRS", "tests")

    conf.check_tool('misc')
    conf.check_tool('gcc')
    conf.check_tool('g++')

    if Options.options.target_platform:
        Options.platform = Options.options.target_platform

    conf.env["VERSION"] = BASEVERSION

    conf.env["CCFLAGS"] = Utils.to_list(conf.env["CCFLAGS"]) + ['-g', '-O0', '-Wall']
    conf.env["CXXFLAGS"] = Utils.to_list(conf.env["CXXFLAGS"]) + ['-g', '-O0', '-Wall']

    if Options.options.bindir:
        conf.env["BINDIR"] = Options.options.bindir
    else:
        conf.env["BINDIR"] = os.path.join(conf.env["PREFIX"], "bin")

    if Options.options.libdir:
        conf.env["LIBDIR"] = Options.options.libdir
    else:
        conf.env["LIBDIR"] = os.path.join(conf.env["PREFIX"], "lib")

    if Options.options.pkgconfigdir:
        conf.env['PKGCONFIGDIR'] = Options.options.pkgconfigdir
        print(conf.env['PKGCONFIGDIR'])
    else:
        conf.env['PKGCONFIGDIR'] = os.path.join(conf.env["PREFIX"], "lib", "pkgconfig")

    if Options.options.enable_gcov:
        conf.env.append_value('CCFLAGS', '-fprofile-arcs')
        conf.env.append_value('CCFLAGS', '-ftest-coverage')
        conf.env.append_value('CXXFLAGS', '-fprofile-arcs')
        conf.env.append_value('CXXFLAGS', '-ftest-coverage')
        conf.env.append_value('LINKFLAGS', '-fprofile-arcs')

    if Options.options.config_prefix:
        for dir in Options.options.config_prefix:
            if not os.path.isabs(dir):
                dir = os.path.abspath(dir)
            conf.env.prepend_value("LIBPATH", os.path.join(dir, "lib"))
            conf.env.prepend_value("CPPPATH", os.path.join(dir, "include"))

    # Our static libraries may link to dynamic libraries
    if Options.platform != 'win32':
        conf.env["staticlib_CCFLAGS"] += ['-fPIC', '-DPIC']
    else:
        # As we have to change target platform after the tools
        # have been loaded there are a few variables that needs
        # to be initiated if building for win32.

        # Make sure we don't have -fPIC and/or -DPIC in our CCFLAGS
        conf.env["shlib_CCFLAGS"] = []

        # Setup various prefixes
        conf.env["shlib_PATTERN"] = 'lib%s.dll'
        conf.env['program_PATTERN'] = '%s.exe'

    # Add some specific OSX things
    if Options.platform == 'darwin':
        conf.env["LINKFLAGS"] += ['-multiply_defined suppress']
        conf.env["explicit_install_name"] = True
    else:
        conf.env["explicit_install_name"] = False

    # Check for support for the generic platform
    has_platform_support = os.name in ('nt', 'posix')
    conf.check_message("platform code for", os.name, has_platform_support)
    if not has_platform_support:
        fatal("s4 only has platform support for Windows "
              "and POSIX operating systems.")
        raise SystemExit(1)

    # Check sunOS socket support
    if Options.platform == 'sunos':
        conf.env.append_unique('CCFLAGS', '-D_POSIX_PTHREAD_SEMANTICS')
        conf.env.append_unique('CCFLAGS', '-D_REENTRANT')
        conf.env.append_unique('CCFLAGS', '-std=gnu99')

    # Glib is required by everyone, so check for it here and let them
    # assume its presence.
    conf.check_cfg(package='glib-2.0', atleast_version='2.8.0', uselib_store='glib2', args='--cflags --libs', mandatory=1)
    conf.check_cfg(package='gthread-2.0', atleast_version='2.6.0', uselib_store='gthread2', args='--cflags --libs')

    subdirs = conf.env["S4_SUBDIRS"]

    for sd in subdirs:
        conf.sub_config(sd)

    newest = max([os.stat(os.path.join(sd, "wscript")).st_mtime for sd in subdirs])
    conf.env['NEWEST_WSCRIPT_SUBDIR'] = newest

    return True


####
## Options
####
def _list_cb(option, opt, value, parser):
    """Callback that lets you specify lists of targets."""
    vals = value.split(',')
    if vals == ['']:
        vals = []
    if getattr(parser.values, option.dest):
        vals += getattr(parser.values, option.dest)
    setattr(parser.values, option.dest, vals)

def set_options(opt):
    opt.add_option('--prefix', default=Options.default_prefix, dest='prefix',
                   help="installation prefix (configuration only) [Default: '%s']" % Options.default_prefix)

    opt.tool_options('gcc')

    opt.add_option('--with-custom-version', type='string',
                   dest='customversion', help="Override git commit hash version")
    opt.add_option('--conf-prefix', action="callback", callback=_list_cb,
                   type='string', dest='config_prefix',
                   help="Specify a directory to prepend to configuration prefix")
    opt.add_option('--with-mandir', type='string', dest='manualdir',
                   help="Specify directory where to install man pages")
    opt.add_option('--with-bindir', type='string', dest='bindir',
                   help="Specify directory where to install executables")
    opt.add_option('--with-libdir', type='string', dest='libdir',
                   help="Specify directory where to install libraries")
    opt.add_option('--with-pkgconfigdir', type='string', dest='pkgconfigdir',
                   help="Specify directory where to install pkg-config files")
    opt.add_option('--with-target-platform', type='string',
                   dest='target_platform',
                   help="Force a target platform (cross-compilation)")
    opt.add_option('--with-windows-version', type='string', dest='winver',
                   help="Force a specific Windows version (cross-compilation)")
    opt.add_option('--build-tests', action='store_true', default=False,
                   dest='build_tests', help="Build test suite")
    opt.add_option('--run-tests', action='store_true', default=False,
                   dest='run_tests', help="Run test suite")
    opt.add_option('--enable-gcov', action='store_true', default=False,
                   dest='enable_gcov', help="Enable code coverage analysis")
    opt.add_option('--lcov-report', action="store_true", default=False,
                   dest='build_lcov', help="Builds an lcov report")

    opt.sub_options("src")

def shutdown():
    if Options.commands['install'] and os.geteuid() == 0:
        ldconfig = '/sbin/ldconfig'
        if os.path.isfile(ldconfig):
            libprefix = Utils.subst_vars('${PREFIX}/lib', Build.bld.env)
            try: Utils.cmd_output(ldconfig + ' ' + libprefix)
            except: pass

    if Options.options.run_tests:
        os.system(os.path.join(blddir, "default/tests/test_s4"))

    if Options.options.build_lcov:
        cd = os.getcwd()
        os.chdir(blddir)
        os.system("lcov -c -b . -d . -o s4.info")
        os.system("genhtml -o ../coverage s4.info")
        os.chdir(cd)
