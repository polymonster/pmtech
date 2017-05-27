import os

def create_info(file):
    modified_time = os.path.getmtime(file)
    return {"name": file, "timestamp": int(modified_time)}
