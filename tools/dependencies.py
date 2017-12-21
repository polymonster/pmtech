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
