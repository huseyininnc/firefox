# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distibuted with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os

import buildconfig
from variables import get_buildid


def main(output, input_file):
    with open(input_file) as fh:
        objs = [l.strip() for l in fh.readlines()]

    write_file(output, None)

    return set(
        os.path.join("build", o)
        for o in objs
        if os.path.splitext(os.path.basename(o))[0] != "buildid"
    )


def tests(output, buildid):
    write_file(output, buildid)


def write_file(output, maybe_buildid):
    buildid = maybe_buildid or get_buildid()
    keyword_extern = "extern" if maybe_buildid is None else ""
    attribute_used = "__attribute__((used))" if maybe_buildid is not None else ""

    output.write(
        f"""
#include "buildid_section.h"

#if defined(XP_DARWIN) || defined(XP_WIN)
#define SECTION_NAME_ATTRIBUTE __attribute__((section(MOZ_BUILDID_SECTION_NAME)))
#else
#define SECTION_NAME_ATTRIBUTE
#endif

{keyword_extern} const char gToolkitBuildID[] SECTION_NAME_ATTRIBUTE {attribute_used} = "{buildid}";
"""
    )

    if buildconfig.substs.get("TARGET_KERNEL") not in (
        "Darwin",
        "WINNT",
    ):
        elf_note = """
#include <elf.h>

#define note_name "mzbldid"
#define note_desc "{buildid}"

// This is not defined on Android?
// Android also hardcodes "1"
// https://android.googlesource.com/platform/ndk/+/refs/tags/ndk-r26c/sources/crt/crtbrand.S#35
#ifndef NT_VERSION
#define NT_VERSION 1
#endif

#if defined(__clang__)
#define NO_SANITIZE_ADDRESS __attribute__((no_sanitize("address")))
#else
// gcc doesn't sanitize address of const variable
#define NO_SANITIZE_ADDRESS
#endif

struct note {{
    Elf32_Nhdr header; // Elf32 or Elf64 doesn't matter, they're the same size
    char name[(sizeof(note_name) + 3) / 4 * 4];
    char desc[(sizeof(note_desc) + 3) / 4 * 4];
}};

{extern} const struct note gNoteToolkitBuildID NO_SANITIZE_ADDRESS __attribute__((section(MOZ_BUILDID_SECTION_NAME), aligned(4), used)) = {{
    {{ sizeof(note_name), sizeof(note_desc), NT_VERSION }},
    note_name,
    note_desc
}};

"""
        output.write(
            "{}".format(
                elf_note.format(
                    extern=keyword_extern,
                    buildid=buildid,
                )
            )
        )
