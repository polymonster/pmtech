import sys
import os
p = os.path.normpath(os.path.join(os.getcwd(), "..", "tools", "build_scripts"))
sys.path.append(p)

import platform
import subprocess
import util

if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmtech tests -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    platform = util.get_platform_name()
    exe_ext = util.get_platform_exe_ext(platform)
    reference_dir = os.path.join(os.getcwd(), "test_reference")
    test_dir = os.path.join(os.getcwd(), "bin", platform)
    test_list = []
    for root, dirs, files in os.walk(reference_dir):
        for file in files:
            test_list.append(os.path.basename(file))
    err = 0
    for test in test_list:
        exe = test.replace(".png", exe_ext)
        exe_path = os.path.join(test_dir, exe)
        cmdline = exe_path + " -test"
        print(cmdline)
        p = subprocess.Popen(cmdline, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        error_code = p.wait()
        output, err = p.communicate()
        output = output.decode('utf-8')
        output = output.strip(" ")
        output = output.split("\n")
        print("running test: " + exe)
        if error_code != 0:
            err = error_code
            print("failed with code: " + str(error_code))
        else:
            print("passed")
    sys.exit(err)


