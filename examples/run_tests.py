import sys
import os
p = os.path.normpath(os.path.join(os.getcwd(), "..", "tools", "build_scripts"))
sys.path.append(p)

import platform
import subprocess
import util
import json
import shutil


# add rpath so apps can find dylibs
def add_rapth(exe):
    if os.path.isdir(exe):
        # app style
        exe = os.path.join(exe, "Contents", "MacOS", exe)
    subprocess.call("install_name_tool -add_rpath \"" + os.getcwd() + "\" " + exe, shell=True)


# run the pmtech samples in test_config.json, compare vs reference image and print results
if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmtech tests -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    platform = util.get_platform_name()
    config = open("test_config.json", "r")
    config = json.loads(config.read())
    exe_ext = util.get_platform_exe_ext(platform)
    exe_run = util.get_platform_exe_run(platform)
    test_dir = os.path.join(os.getcwd(), "bin", platform)
    reference_dir = os.path.join(os.getcwd(), "test_reference")
    nvtt = os.path.join(os.getcwd(), "..", "tools", "bin", "nvtt", platform, "nvcompress")
    os.chdir(test_dir)
    output_dirs = ["test_results", "test_reference"]
    for dir in output_dirs:
        if not os.path.exists(dir):
            os.mkdir(dir)
    # check if we want to generate the reference image set
    generate = False
    if len(sys.argv) > 1:
        if sys.argv[1] == "-generate":
            generate = True
            # remove source pngs, built dds, test result png and text
            test_data_dirs = [reference_dir, "test_results", "test_reference"]
            for dir in test_data_dirs:
                for root, dirs, files in os.walk(dir):
                    for file in files:
                        os.remove(os.path.join(root, file))
    exit_code = 0
    for test in config["tests"]:
        exe = test["name"] + exe_ext
        ref = os.path.join(reference_dir, exe.replace(".exe", ".png"))
        ref_dds = os.path.join("test_reference", exe.replace(".exe", ".dds"))
        # generate dds reference image for easier loading
        if os.path.exists(ref):
            dds_cmd = exe_run + nvtt + " -rgb -alpha -nomips -silent " + ref + " " + ref_dds
            p = subprocess.Popen(dds_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            error_code = p.wait()
        if not os.path.exists(exe):
            continue
        if platform == "osx":
            add_rapth(exe)
        # if the test exe exists run the test
        cmdline = exe_run + exe + " -test"
        p = subprocess.Popen(cmdline, shell=True)
        error_code = p.wait()
        # process results, we do this here because appveyor is not getting stdout from c++ programs
        if not generate:
            results_file = os.path.join("test_results", test["name"] + ".txt")
            if not os.path.exists(results_file):
                print("missing test results: " + results_file)
                continue
            results = open(results_file)
            results = json.loads(results.read())
            print("--------------------------------------------------------------------------------")
            print("running test: " + exe)
            if error_code != 0:
                exit_code = error_code
                print("program failed with code: " + str(error_code))
            else:
                diffs = str(results["diffs"])
                tested = str(results["tested"])
                percentage = str(results["percentage"])
                print(diffs + "/" + tested + " diffs(" + percentage + "%)")
                if results["percentage"] < test["diff threshold"]:
                    print("passed")
                else:
                    print("failed")
                    exit_code = 1
            print("--------------------------------------------------------------------------------")

    sys.exit(exit_code)


