import os
import json


def correct_filename(file):
    file = file.replace("\\", '/')
    file = file.replace(":", "@")
    return file


def create_info(file):
    modified_time = os.path.getmtime(file)
    file = correct_filename(file)
    return {"name": file, "timestamp": int(modified_time)}


def create_dependency_info(inputs, outputs):
    info = dict()
    for o in outputs:
        o = correct_filename(o)
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
                    return False
    return True


def write_to_file(dependencies):
    dir = dependencies["dir"]
    directory_dependencies = os.path.join(dir, "dependencies.json")
    output_d = open(directory_dependencies, 'wb+')
    output_d.write(bytes(json.dumps(dependencies, indent=4), 'UTF-8'))
    output_d.close()
