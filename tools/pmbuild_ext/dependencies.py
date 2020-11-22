import os
import json

default_settings = dict()
default_settings["textures_dir"] = "assets/textures/"
default_settings["models_dir"] = "assets/mesh/"


def delete_orphaned_files(build_dir, platform_data_dir):
    for root, dir, files in os.walk(build_dir):
        for file in files:
            dest_file = os.path.join(root, file)
            if dest_file.find("dependencies.json") != -1:
                depends_file = open(dest_file, "r")
                depends_json = json.loads(depends_file.read())
                depends_file.close()
                for file_dependencies in depends_json["files"]:
                    for key in file_dependencies.keys():
                        for dependency_info in file_dependencies[key]:
                            if not os.path.exists(dependency_info["name"]):
                                del_path = os.path.join(platform_data_dir, key)
                                if os.path.exists(del_path):
                                    os.remove(os.path.join(platform_data_dir, key))
                                    print("deleting " + key + " source file no longer exists")
                                    print(del_path)
                                    break


def get_build_config_setting(dir_name):
    if os.path.exists("build_config.json"):
        build_config_file = open("build_config.json", "r")
        build_config_json = json.loads(build_config_file.read())
        build_config_file.close()
        if dir_name in build_config_json:
            return build_config_json[dir_name]
    return default_settings[dir_name]


def export_config_merge(master, second):
    for key in master.keys():
        if key in second.keys():
            master[key] = export_config_merge(master[key], second[key])
    for key in second.keys():
        if key not in master.keys():
            master[key] = second[key]
    return master


def get_export_config(filename):
    export_info = dict()
    rpath = filename.replace(os.getcwd(), "")
    rpath = os.path.normpath(rpath)
    sub_dirs = rpath.split(os.sep)
    full_path = os.getcwd()
    for dir in sub_dirs:
        full_path = os.path.join(full_path, dir)
        dir_export_file = os.path.join(full_path, "_export.json")
        if os.path.exists(dir_export_file):
            file = open(dir_export_file, "r")
            file_json = file.read()
            dir_info = json.loads(file_json)
            export_info = export_config_merge(export_info, dir_info)
    return export_info


def sanitize_filename(filename):
    sanitized_name = filename.replace("@", ":")
    sanitized_name = sanitized_name.replace('/', os.sep)
    return sanitized_name


def create_info(file):
    file = sanitize_filename(file)
    file = os.path.normpath(os.path.join(os.getcwd(), file))
    modified_time = os.path.getmtime(file)
    return {"name": file, "timestamp": float(modified_time)}


def create_dependency_info(inputs, outputs, cmdline=""):
    info = dict()
    info["cmdline"] = cmdline
    info["files"] = dict()
    for o in outputs:
        o = os.path.join(os.getcwd(), o)
        info["files"][o] = []
        for i in inputs:
            if not os.path.exists(i):
                continue
            ii = create_info(i)
            ii["data_file"] = o[o.find(os.sep + "data" + os.sep) + 1:]
            info["files"][o].append(ii)
    return info


def check_up_to_date(dependencies, dest_file):
    filename = os.path.join(dependencies["dir"], "dependencies.json")
    if not os.path.exists(filename):
        print("depends does not exist")
        return False
    file = open(filename)
    d_str = file.read()
    d_json = json.loads(d_str)
    file_exists = False
    for d in d_json["files"]:
        for key in d.keys():
            dependecy_file = sanitize_filename(key)
            if dest_file == dependecy_file:
                for i in d[key]:
                    file_exists = True
                    sanitized = sanitize_filename(i["name"])
                    if not os.path.exists(sanitized):
                        return False
                    if i["timestamp"] < os.path.getmtime(sanitized):
                        return False
    if not file_exists:
        return False
    return True


def check_up_to_date_single(dest_file, deps):
    dest_file = sanitize_filename(dest_file)
    dep_filename = dest_file.replace(os.path.splitext(dest_file)[1], ".dep")
    if not os.path.exists(dep_filename):
        print(os.path.basename(dest_file) + ": deps does not exist.")
        return False
    dep_ts = os.path.getmtime(dest_file)
    file = open(dep_filename)
    d_str = file.read()
    d_json = json.loads(d_str)
    # check for changes to cmdline
    if "cmdline" in deps:
        if "cmdline" not in d_json.keys() or deps["cmdline"] != d_json["cmdline"]:
            print(dest_file + " cmdline changed")
            return False
    # check for new additions
    dep_files = []
    for output in d_json["files"]:
        for i in d_json["files"][output]:
            dep_files.append(i["name"])
    for output in deps["files"]:
        for i in deps["files"][output]:
            if i["name"] not in dep_files:
                print(os.path.basename(dest_file) + ": has new inputs")
                return False
    # check for timestamps on existing
    for d in d_json["files"]:
        dest_file = sanitize_filename(d)
        for input_file in d_json["files"][d]:
            # output file does not exist yet
            if not os.path.exists(dest_file):
                print(os.path.basename(dest_file) + ": does not exist.")
                return False
            # output file is out of date
            if os.path.getmtime(input_file["name"]) > dep_ts:
                print(os.path.basename(dest_file) + ": is out of date.")
                return False
    print(os.path.basename(dest_file) + " up to date")
    return True


def write_to_file(dependencies):
    dir = dependencies["dir"]
    directory_dependencies = os.path.join(dir, "dependencies.json")
    try:
        output_d = open(directory_dependencies, 'wb+')
        output_d.write(bytes(json.dumps(dependencies, indent=4), 'UTF-8'))
        output_d.close()
    except:
        return


def write_to_file_single(deps, file):
    output_d = open(file, 'wb+')
    output_d.write(bytes(json.dumps(deps, indent=4), 'UTF-8'))
    output_d.close()
