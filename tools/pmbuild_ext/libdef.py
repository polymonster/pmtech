#!/usr/bin/env python
#
# This script was enspired by https://github.com/microsoft/llvm/blob/master/utils/extract_symbols.py
#
# If you miss more performance (huge libraries), makes sense to check that python script though
#
import sys
import re
import os
import subprocess
import argparse

# Define functions which extract a list of symbols from a library using several
# different tools. We use subprocess.Popen and yield a symbol at a time instead
# of using subprocess.check_output and returning a list as, especially on
# Windows, waiting for the entire output to be ready can take a significant
# amount of time.
def dumpbin_get_symbols(lib):

    # total hack adding paths
    dumps = [
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional\\VC\\Tools\\MSVC\\14.16.27023\\bin\\Hostx64\\x64\dumpbin.exe",
        'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.28.29333\\bin\\Hostx64\\x64\\dumpbin.exe'
    ]
    
    dumpbin_exe = ""
    for d in dumps:
        if os.path.isfile(d):
            dumpbin_exe = d
            break

    if not os.path.isfile(dumpbin_exe):
        print("error: dumpbin utility not found, please reconfigure path\n Path: " + dumpbin_exe)
        exit(2)
    
    process = subprocess.Popen([dumpbin_exe,'/symbols',lib], bufsize=1,
                               stdout=subprocess.PIPE, stdin=subprocess.PIPE,
                               universal_newlines=True)
    process.stdin.close()
    for line in process.stdout:
        # Look for external symbols that are defined in some section
        match = re.match("^.+SECT.+External\s+\|\s+(\S+).*$", line)
        if match:
            yield match.group(1)
    process.wait()

# MSVC mangles names to ?<identifier_mangling>@<type_mangling>. By examining the
# identifier/type mangling we can decide which symbols could possibly be
# required and which we can discard.
#
# Mangling scheme could be looked in clang/lib/AST/MicrosoftMangle.cpp
#
def should_keep_microsoft_symbol(symbol):
    # Variables with 'g_' prefix (globals)
    if re.search('g_', symbol):
        return symbol + " DATA"
        
    namespaces = [
        "pen",
        "put",
        "Str",
        "ImGui",
        "physics"
    ]
    
    # We are intrested only in our functionality, not standard libraries
    valid = False
    for n in namespaces:
        if re.search(n, symbol):
            valid = True
            break
    
    if not valid:
        return None

    # mangleVariableEncoding => public static or global member
    if re.search('@@[23]', symbol):
        return symbol + " DATA"

    return symbol

def extract_symbols(lib):
    symbols = dict()
    for symbol1 in dumpbin_get_symbols(lib):
        symbol = should_keep_microsoft_symbol(symbol1)
        
        if symbol:
            print("accepting symbol: " + symbol)
        #else:
        #    print("rejecting symbol: " + symbol1)
        
        if symbol:
            symbols[symbol] = 1 + symbols.setdefault(symbol,0)
    return symbols


if __name__ == '__main__':
    print ("Executed command:\n  >" + ' '.join(sys.argv) + "\n")

    parser = argparse.ArgumentParser(description='Extracts symbols from static library and saves as a .def file')
    parser.add_argument('libs', metavar='lib', type=str, nargs='+', help='libraries to extract symbols from')
    parser.add_argument('-o', metavar='file', type=str, help='output to file')
    args = parser.parse_args()

    # Get the list of libraries to extract symbols from
    libs = list()
    for lib in args.libs:
        # When invoked by cmake the arguments are the cmake target names of the
        # libraries, so we need to add .lib/.a to the end and maybe lib to the
        # start to get the filename. Also allow objects.
        suffixes = ['.lib','.a','.obj','.o']
        if not any([lib.endswith(s) for s in suffixes]):
            for s in suffixes:
                if os.path.exists(lib+s):
                    lib = lib+s
                    break
                if os.path.exists('lib'+lib+s):
                    lib = 'lib'+lib+s
                    break
        if not any([lib.endswith(s) for s in suffixes]):
            print("Don't know what to do with argument "+lib, file=sys.stderr)
            exit(3)
        libs.append(lib)


    # Merge everything into a single dict
    symbols = dict()

    for lib in libs:
        lib_symbols = extract_symbols(lib)

        for k,v in list(lib_symbols.items()):
            symbols[k] = v + symbols.setdefault(k,0)

    if args.o:
        outfile = open(args.o,'w')
    else:
        outfile = sys.stdout

    print("EXPORTS", file=outfile)

    for k,v in list(symbols.items()):
        print(k, file=outfile)
        #if k.endswith(" DATA"):
        #    print("__imp_" + k, file=outfile)

    #print("SECTIONS", file=outfile)
    #print("   .idata        READ WRITE", file=outfile)