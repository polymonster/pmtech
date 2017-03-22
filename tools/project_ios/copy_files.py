import os
import os.path
import shutil

solution_dir = os.getcwd()

f = open("premake5.lua", "r")
contents = f.read()

found = 1
lua_len = len(contents)
app_list = []
pos = 0

strip_chars = [" ", ",", "\""]

while(found):
    pos = contents.find("create_app",pos,lua_len)
    if pos != -1:
        name_pos = contents.find("(",pos,lua_len) + 1
        pos_end = contents.find(",", name_pos, lua_len) - 1
        app_list.append(contents[name_pos:pos_end])
        pos = pos_end
    else:
        found = 0

cleaned_app_list = []

for app in app_list:
    new_str = app
    for char in strip_chars:
        new_str = new_str.strip(char)
    cleaned_app_list.append(new_str)

for app in cleaned_app_list:
    ios_files_path = (solution_dir + "/" + app + "/ios_files")
    if not os.path.exists(ios_files_path):
        print("copy ios files for " + app )
        shutil.copytree("../tools/project_ios/ios_files", ios_files_path)









