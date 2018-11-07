import subprocess
import os
import json
import sys


def display_help():
    print("generation icons help:")
    print("\t-platform <macos, win32, ios, linux, android>")
    print("\t-files <list of source files>")
    print("\n")
    print("source files must be square and .png format. A single 1024x1024 image is recommended")
    print("smaller images can be supplied if necessary to improve quality at small sizes")
    print("to generate an icon for any size ")


def get_icon_set(platform):
    file = open(platform + ".json", "r")
    icon_set = json.loads(file.read())
    file.close()
    return icon_set


def get_png_dimensions(filename):
    file = open(filename, "rb")
    magic_magic = [137, 80, 78, 71, 13, 10, 26, 10]
    magic = file.read(24)
    for i in range(8):
        if magic[i] != magic_magic[i]:
            print("error: file is not png!")
    w = int.from_bytes(magic[16:20], "big")
    h = int.from_bytes(magic[20:24], "big")
    file.close()
    return w, h


def find_best_image(file_list, dest_img):
    target = dest_img["size"]
    target = target[:target.find("x")]
    target = int(target)
    diff = 65535
    best_file = ""
    best_size = 0
    for file in file_list:
        w, h = get_png_dimensions(file)
        d = w - target
        if d < diff and d > 0:
            diff = d
            best_file = file
            best_size = w
    scale = float(target) / float(best_size)
    return best_file, scale


if __name__ == "__main__":
    platform = ""
    file_list = []
    valid = True
    for i in range(len(sys.argv)):
        if sys.argv[i] == "-help":
            break
        elif sys.argv[i] == "-platform":
            platform = sys.argv[i+1]
            i = i+2
        elif sys.argv[i] == "-files":
            i += 1
            while i < len(sys.argv):
                file_list.append(sys.argv[i])
                i += 1
    output_dir = "test"
    os.mkdir(output_dir)
    if platform != "" and len(file_list) > 0:
        icon_set = get_icon_set(platform)
        for dest_img in icon_set["images"]:
            file, scale = find_best_image(file_list, dest_img)
            out_file = dest_img["idiom"] + "_" + dest_img["size"] + "_" + dest_img["scale"] + ".png"
            out_file = os.path.join(output_dir, out_file)
            nvzoom = os.path.join('..', "bin", "nvtt", "osx", "nvzoom")
            cmdline = nvzoom + " -s " + str(scale) + " " + file + " " + out_file
            print(cmdline)
            subprocess.call(cmdline, shell=True)
    else:
        display_help()