import os

def create_info(file):
    modified_time = os.path.getmtime(file)
    file = file.replace("\\", '/')
    file = file.replace(":", "@")
    return {"name": file, "timestamp": int(modified_time)}
