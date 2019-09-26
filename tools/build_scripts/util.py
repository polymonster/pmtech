import platform
import os
import shutil


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


def get_platform_exe_ext(platform):
    if platform == "win32":
        return ".exe"
    else:
        return ""


def get_platform_exe_run(platform):
    if platform == "win32":
        return ""
    else:
        return "./"


# create a new dir if it doesnt already exist and not throw an exception
def create_dir(dst_file):
    dir = os.path.dirname(dst_file)
    if not os.path.exists(dir):
        os.makedirs(dir)


# copy src_file to dst_file creating directory if necessary
def copy_file_create_dir(src_file, dst_file):
    if not os.path.exists(src_file):
        print("[error] " + src_file + " does not exist!")
        return False
    try:
        create_dir(dst_file)
        src_file = os.path.normpath(src_file)
        dst_file = os.path.normpath(dst_file)
        shutil.copyfile(src_file, dst_file)
        print("copy " + src_file + " to " + dst_file)
        return True
    except Exception as e:
        print("[error] failed to copy " + src_file)
        return False


# copy src_file to dst_file creating directory if necessary only if the src file is newer than dst
def copy_file_create_dir_if_newer(src_file, dst_file):
    if not os.path.exists(src_file):
        print("[error] src_file " + src_file + " does not exist!")
        return
    if os.path.exists(dst_file):
        if os.path.getmtime(dst_file) >= os.path.getmtime(src_file):
            print(dst_file + " up-to-date")
            return
    copy_file_create_dir(src_file, dst_file)


# member wise merge 2 dicts, second will overwrite dest
def merge_dicts(dest, second):
    for k, v in second.items():
        if type(v) == dict:
            if k not in dest or type(dest[k]) != dict:
                dest[k] = dict()
            merge_dicts(dest[k], v)
        else:
            dest[k] = v


# change file extension to ext
def change_ext(file, ext):
    return os.path.splitext(file)[0] + ext


if __name__ == "__main__":
    print("util")
