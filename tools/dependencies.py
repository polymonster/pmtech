import os
import json


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
    # print(json.dumps(export_info, indent=4, separators=(',', ': ')))
    return export_info

def unstrict_json_safe_filename(file):
    file = file.replace("\\", '/')
    file = file.replace(":", "@")
    return file

def sanitize_filename(filename):
    sanitized_name = filename.replace("@", ":")
    sanitized_name = sanitized_name.replace('/', os.sep)
    return sanitized_name

def create_info(file):
    file = sanitize_filename(file)
    modified_time = os.path.getmtime(file)
    file = unstrict_json_safe_filename(file)
    return {"name": file, "timestamp": float(modified_time)}


def create_dependency_info(inputs, outputs):
    info = dict()
    for o in outputs:
        o = unstrict_json_safe_filename(o)
        info[o] = []
        for i in inputs:
            info[o].append(create_info(i))
    return info


def check_up_to_date(dependencies, dest_file):
    filename = os.path.join(dependencies["dir"], "dependencies.json")
    if not os.path.exists(filename):
        print("depends does not exist")
        return False

    file = open(filename)
    d_str = file.read()
    d_json = json.loads(d_str)

    for d in d_json["files"]:
        if dest_file in d.keys():
            for i in d[dest_file]:
                if i["timestamp"] < os.path.getmtime(i["name"]):
                    print(i["name"] + " " + str(os.path.getmtime(i["name"])))
                    print(str(i["timestamp"]))
                    return False
    return True


def write_to_file(dependencies):
    dir = dependencies["dir"]
    directory_dependencies = os.path.join(dir, "dependencies.json")
    output_d = open(directory_dependencies, 'wb+')
    output_d.write(bytes(json.dumps(dependencies, indent=4), 'UTF-8'))
    output_d.close()
