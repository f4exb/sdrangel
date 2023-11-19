#!/usr/bin/python
"""
Utility to generate copyright notices from the git history of the source file
Inspired by: https://0pointer.net/blog/projects/copyright.html
"""
import sys
import os
import functools
from subprocess import *
from datetime import *
from optparse import OptionParser

AUTHOR_SUBSTITUTES = {
    "ZigaS": "Ziga S",
    "hexameron": "John Greb",
    "srcejon": "Jon Beniston, M7RCE",
    "Jon Beniston": "Jon Beniston, M7RCE",
    "f4exb": "Edouard Griffiths, F4EXB",
    "Edouard Griffiths": "Edouard Griffiths, F4EXB"
}

# Commits rewriting the copyright notices
EXCLUDE_HASHES = {
    "c9e13c336",
    "e61317ef0",
    "65842d9b5",
    "00b041d76",
    "3a944fa20",
    "b6c4d10b6",
    "869f1a419",
    "743260db9",
    "9ce0810a2",
    "3596fe431"
}

# ======================================================================
def getInputOptions():
    parser = OptionParser(usage="usage: %%prog options\n\n%s")
    parser.add_option("-f", "--file", dest="file", help="File to process", metavar="FILE", type="str")
    parser.add_option("-d", "--directory", dest="directory", help="Directory to process", metavar="DIRECTORY", type="str")
    parser.add_option("-e", "--extension", dest="extensions", help="Filter by this extension (includes dot)", metavar="EXTENSION", type="str", action="append")
    parser.add_option("-l", "--list-authors", dest="list_authors", help="List authors", metavar="LIST", action="store_true", default=False)
    parser.add_option("-r", "--remove-original", dest="remove_original", help="Remove original copyright notices", metavar="REMOVE", action="store_true", default=False)
    parser.add_option("-n", "--dry-run", dest="dry_run", help="Print new headers instead of overwriting the files", metavar="DRY_RUN", action="store_true", default=False)
    (options, args) = parser.parse_args()
    return options

# ======================================================================
def validate_options(options):
    if options.file is None and options.directory is None:
        print("At least a file (-f) or a directory (-d) must be specified")
        return False
    elif options.file is not None and options.directory is not None:
        print("Specify either a file (-f) or a directory (-d) but not both")
        return False
    if not options.extensions:
        options.extensions = [".h", ".cpp"]
    return True

# ======================================================================
def pretty_years(s):

    l = list(s)
    l.sort()

    start = None
    prev = None
    r = []

    for x in l:
        if prev is None:
            start = x
            prev = x
            continue

        if x == prev + 1:
            prev = x
            continue

        if prev == start:
            r.append("%i" % prev)
        else:
            r.append("%i-%i" % (start, prev))

        start = x
        prev = x

    if not prev is None:
        if prev == start:
            r.append("%i" % prev)
        else:
            r.append("%i-%i" % (start, prev))

    return ", ".join(r)

# ======================================================================
def order_by_year(a, b):

    la = list(a[2])
    la.sort()

    lb = list(b[2])
    lb.sort()

    if la[0] < lb[0]:
        return -1
    elif la[0] > lb[0]:
        return 1
    else:
        return 0

# ======================================================================
def analyze(f):
    print(f"File: {f}")

    commits = []
    data = {}

    for ln in Popen(["git", "log", "--follow", "--all", "--date=format:'%Y-%m-%d %H:%M:%S,%z'", "--pretty=format:'%an,%ae,%ad,%h'", f], stdout=PIPE).stdout:
        ls = ln.decode().strip()
        le = ls.split(',')       # Line elements (comma separated)
        lh = le[4].rstrip("\'")
        if lh in EXCLUDE_HASHES:
            continue
        dt = datetime.strptime(le[2].lstrip("\'"), '%Y-%m-%d %H:%M:%S')
        tz = le[3].rstrip("\'")
        data = {
            "author": le[0].lstrip("\'"),
            "author-mail": le[1],
            "author-time": int(datetime.timestamp(dt)),
            "author-tz": tz
        }
        if data["author"] == "Hexameron":
            data["author-time"] = int(datetime.timestamp(datetime(2012, 1, 1)))
        if data["author"] in AUTHOR_SUBSTITUTES:
            data["author"] = AUTHOR_SUBSTITUTES[data["author"]]
        commits.append(data)

    by_author = {}

    for c in commits:
        try:
            n =  by_author[c["author"]]
        except KeyError:
            n = (c["author"], c["author-mail"], set())
            by_author[c["author"]] = n

        # FIXME: Handle time zones properly
        year = datetime.fromtimestamp(int(c["author-time"])).year

        n[2].add(year)

    for an, a in list(by_author.items()):
        for bn, b in list(by_author.items()):
            if a is b:
                continue

            if a[1] == b[1]:
                a[2].update(b[2])

                if an in by_author and bn in by_author:
                    del by_author[bn]

    copyrite = list(by_author.values())
    copyrite.sort(key=functools.cmp_to_key(order_by_year))
    return copyrite

# ======================================================================
def get_files(options):
    files = []
    dirs = os.walk(options.directory)
    for dirspec in dirs:
        for f in dirspec[2]:
            filepath = os.path.join(dirspec[0], f)
            ext = os.path.splitext(filepath)[1]
            if ext in options.extensions:
                files.append(filepath)
    return files

# ======================================================================
def list_authors(options):
    authors = set()
    if options.directory is not None:
        files = get_files(options)
        for f in files:
            copyrite = analyze(f)
            for c in copyrite:
                authors.add(c[0])
        for author in authors:
            print(author)
        return
    copyrite = analyze(options.file)
    for c in copyrite:
        print(c[0])

# ======================================================================
def remove_line(line):
    if "Copyright (C)" in line:
        return True
    if "Copyright (c)" in line:
        return True
    if line.startswith("// written by"):
        return True

# ======================================================================
def get_header_lines():
    return [
        "",
        "This program is free software; you can redistribute it and/or modify",
        "it under the terms of the GNU General Public License as published by",
        "the Free Software Foundation as version 3 of the License, or",
        "(at your option) any later version.",
        "",
        "This program is distributed in the hope that it will be useful,",
        "but WITHOUT ANY WARRANTY; without even the implied warranty of",
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the",
        "GNU General Public License V3 for more details.",
        "",
        "You should have received a copy of the GNU General Public License",
        "along with this program. If not, see <http://www.gnu.org/licenses/>."
    ]

# ======================================================================
def process_file(f, options):
    with open(f) as ff:
        lines_in = [line.rstrip() for line in ff]
    lines_out = []
    cr = analyze(f)
    header = False
    header_start = 0
    for iline, line in enumerate(lines_in):
        if line.startswith("////////"):
            header= True
            header_start = iline
            break
    if header:
        lines_out = lines_in[:header_start+1]
    else:
        lines_out = ["///////////////////////////////////////////////////////////////////////////////////////"]
    width = len(lines_out[header_start]) - 6
    for name, mail, years in cr:
        if name == "Hexameron":
            lines_out.append("// {0:{1}} //".format("Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany", width))
            lines_out.append("// {0:{1}} //".format("written by Christian Daniel", width))
        else:
            cr_string = f"Copyright (C) {pretty_years(years)} {name} <{mail}>"
            lines_out.append(f"// {cr_string:{width}} //")
    if not header:
        for hline in get_header_lines():
            lines_out.append("// {0:{1}} //".format(hline, width))
        lines_out.append(lines_out[0])
    elif not options.remove_original:
        lines_out.append("")
    in_header = header
    header_stop = len(lines_out)
    iread = header_start+1 if header else 0
    for iline, line in enumerate(lines_in[iread:]):
        if in_header and options.remove_original and remove_line(line):
            continue
        if line.startswith("////////"):
            in_header = False
            header_stop += iline
        lines_out.append(line)
    if options.dry_run:
        for line_out in lines_out[header_start:header_stop]:
            print(line_out)
    else:
        with open(f, "w") as ff:
            for line in lines_out:
                ff.write(f"{line}\n")


# ======================================================================
def process_directory(options):
    files = get_files(options)
    for f in files:
        process_file(f, options)


# ======================================================================
def main():
    try:
        options = getInputOptions()
        if not validate_options(options):
            sys.exit(-1)
        if options.list_authors:
            list_authors(options)
        elif options.file:
            process_file(options.file, options)
        else:
            process_directory(options)
    except KeyboardInterrupt:
        print("Keyboard interrupt. Exiting")


# ======================================================================
if __name__ == '__main__':
    main()
