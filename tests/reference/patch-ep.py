#!/usr/bin/env python3
"""Turns ElectroPortis' ep.c into ep_ref.c for the Linux reference build.

Only the build environment changes: the Windows headers are replaced by
ep_shim.h, the MSVC-only trunc() goes, the table of 32-bit actor pointers
gets 32-bit entries again (ep.c reads it by byte offset), the random seed
comes from the test instead of time(), and the test can replace the script.
The decompiled code itself is untouched.

usage: patch-ep.py ep.c ep_ref.c
"""
import re
import sys


def replace_once(text, old, new):
    if text.count(old) != 1:
        sys.exit("patch-ep.py: expected exactly one %r" % old)
    return text.replace(old, new)


def main():
    source, target = sys.argv[1], sys.argv[2]
    text = open(source, encoding="latin-1").read()

    text = replace_once(text,
        '#include "stdafx.h"\n#include "ep.h"\n#include "replay_draw.hpp"\n'
        '#include "saver.hpp"\n',
        '#include "ep_shim.h"\n')

    text, count = re.subn(
        r'#if _MSC_VER < 1800\nstatic double\ntrunc\(double d\)\n\{\n.*?\n\}\n'
        r'#endif // _MSC_VER\n', '', text, flags=re.S)
    if count != 1:
        sys.exit("patch-ep.py: trunc() block not found")

    text = replace_once(text, 'static uintptr_t acttable[1024];',
        'static uint32_t acttable[1024];')
    text = replace_once(text, 'static uintptr_t *acttable_ptr = acttable;',
        'static uint32_t *acttable_ptr = acttable;')
    text = replace_once(text, 'srand48(time(NULL));', 'srand48(ep_ref_seed);')

    text += '''
/* Added for the reference test. */
long ep_ref_seed;

const char *
ep_ref_default_script(void)
{
    return defaultScript;
}

void
ep_ref_set_script(char *script)
{
    defaultScript = script;
}
'''
    open(target, "w", encoding="latin-1").write(text)


main()
