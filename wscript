# -*- python -*-

import os

APPNAME = 'mapred'
VERSION = '0.1'

top = '.'
out = 'build'

def configure(conf):
    conf.load('compiler_c')
    conf.load('compiler_cxx')

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    opt.add_option('--mode', action='store', default='release', help='Compile mode: release or debug')
    opt.add_option('--san', action='store', default=None, help='Enable specific sanitizer')

def build(bld):
    cflags = {
        'cflags'        : ['-O2', '-Wall', '-Wextra', '-Werror', '-std=gnu11', '-D_GNU_SOURCE'],
    }
    cxxflags = {
        'cxxflags'      : ['-O2', '-Wall', '-Wextra', '-Werror', '-std=gnu++14'],
    }
    if bld.options.mode == 'debug' or bld.options.san:
        cflags['cflags'] += ['-g', '-O0']
    if bld.options.san:
        opt = '-fsanitize=%s' % bld.options.san
        cflags['cflags'] += [opt]
        cflags['ldflags'] = [opt]
    source = bld.path.ant_glob('src/*.c', excl=['**/%s_*.c' % x for x in ['test', 'bench']]+['**/main.c'])

    features = {
        'source'        : source,
        'target'        : 'sh-mapred',
    }
    features.update(cflags)
    bld.shlib(**features)

    features = {
        'source'        : source,
        'target'        : 'st-mapred',
    }
    features.update(cflags)
    bld.stlib(**features)

    for test in bld.path.ant_glob('src/test_*.c'):
        features = {
            'source'        : [test],
            'target'        : os.path.splitext(str(test))[0],
            'use'           : 'st-mapred',
            'lib'           : ['cunit', 'crypto', 'pthread'],
        }
        features.update(cflags)
        bld.program(**features)

    for bench in bld.path.ant_glob('src/bench_*.cc'):
        features = {
            'source'        : [bench],
            'target'        : os.path.splitext(str(bench))[0],
            'use'           : 'st-mapred',
            'lib'           : ['crypto'],
            'stlib'         : ['hayai_main'],
            'lib'           : ['pthread'],
        }
        if bld.options.san:
            features['lib'] += ['ubsan', 'asan', 'tsan']
        features.update(cxxflags)
        bld.program(**features)

    features = {
        'source'        : ['src/main.c'],
        'target'        : 'mapred',
        'use'           : 'st-mapred',
        'lib'           : ['crypto', 'pthread'],
    }
    features.update(cflags)
    bld.program(**features)
