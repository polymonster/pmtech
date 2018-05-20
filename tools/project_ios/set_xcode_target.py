import os


def set_xcode_target(xcode_file):
    print("updating " + xcode_file)
    f = open(xcode_file, 'r')
    contents = f.read()
    f.close()

    f = open(xcode_file, 'w')
    contents = contents.replace("ARCHS = \"$(NATIVE_ARCH_ACTUAL)\";\n",
                                "ARCHS = \"$(NATIVE_ARCH_ACTUAL)\";\n SDKROOT = iphoneos10.2;\n")

    f.write(contents)
    f.close()


for root, dirs, files in os.walk("../"):
    for file in files:
        if file.endswith(".pbxproj") and "ios" in root:
            file_and_path = os.path.join(root, file)
            set_xcode_target(file_and_path)
