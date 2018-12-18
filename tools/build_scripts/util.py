import platform
import os


def get_platform_name_args(args):
    for i in range(1, len(args)):
        if "-platform" in args[i]:
            return args[i + 1]
    plat = "win32"
    if os.name == "posix":
        plat = "osx"
        if platform.system() == "Linux":
            plat = "linux"
    return plat


def get_platform_name():
    plat = "win32"
    if os.name == "posix":
        plat = "osx"
        if platform.system() == "Linux":
            plat = "linux"
    return plat


def correct_path(path):
    if os.name == "nt":
        return path.replace("/", "\\")
    return path


def sanitize_file_path(path):
    path = path.replace("/", os.sep)
    path = path.replace("\\", os.sep)
    return path


if __name__ == "__main__":
    print("util")
