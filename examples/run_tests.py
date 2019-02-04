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
    exit_code = 0
    os.chdir(test_dir)
    for test in test_list:
        exe = test.replace(".png", exe_ext)
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
        for e in output:
            if e != "":
                print(e)
        if error_code != 0:
            exit_code = error_code
            print("failed with code: " + str(error_code))
        else:
            print("passed")
    sys.exit(exit_code)


