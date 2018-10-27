import subprocess
import os


def display_help():
    print("generation icons help:")
    print("\t-platform <osx, win32, ios, linux, android>")
    print("\t-files <list of source files>")


if __name__ == "__main__":
    nvzoom = os.path.join('..', "bin", "nvtt", "osx", "nvzoom")
    cmdline = nvzoom + " -s 0.15 " + " lena_std.tga " + " small.tga"
    subprocess.call(cmdline, shell=True)