import sys
import os
p = os.path.normpath(os.path.join(os.getcwd(), "..", "tools", "build_scripts"))
sys.path.append(p)

import platform
import subprocess
import util
import json
import shutil

if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmtech tests -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    platform = util.get_platform_name()
    config = open("test_config.json", "r")
    config = json.loads(config.read())
    exe_ext = util.get_platform_exe_ext(platform)
    test_dir = os.path.join(os.getcwd(), "bin", platform)
    reference_dir = os.path.join(os.getcwd(), "test_reference")
    nvtt = os.path.join(os.getcwd(), "..", "tools", "bin", "nvtt", platform, "nvcompress")
    os.chdir(test_dir)
    output_dirs = ["test_results", "test_reference"]
    for dir in output_dirs:
        if not os.path.exists(dir):
            os.mkdir(dir)
    if len(sys.argv) > 1:
        if sys.argv[1] == "-generate":
            for root, dirs, files in os.walk(reference_dir):
                for file in files:
                    os.remove(os.path.join(root, file))
    exit_code = 0
    for test in config["tests"]:
        exe = test["name"] + exe_ext
        ref = os.path.join(reference_dir, exe.replace(".exe", ".png"))
        ref_dds = os.path.join("test_reference", exe.replace(".exe", ".dds"))
        # generate dds reference image
        if os.path.exists(ref):
            dds_cmd = nvtt + " " + ref + " " + ref_dds
            print(dds_cmd)
            subprocess.call(dds_cmd, shell=True)
        if not os.path.exists(exe):
            continue
        cmdline = exe + " -test"
        p = subprocess.Popen(cmdline, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        error_code = p.wait()
        output, err = p.communicate()
        output = output.decode('utf-8')
        output = output.strip(" ")
        output = output.split("\n")
        print("running test: " + exe)
        if error_code != 0:
            exit_code = error_code
            print("failed with code: " + str(error_code))
        else:
            print("passed")
    sys.exit(exit_code)


