file = open("dev_ui_icons.h")
file_text = file.read()
file_lines = file_text.split('\n')

debug_icons = []
for line in file_lines:
    line_tok = line.split(' ')
    if line_tok > 1:
        if line_tok[0] == "#define":
            if "ICON_FA_" in line_tok[1]:
                debug_icons.append("\tICON_DEBUG(" + line_tok[1] + ");\n")

debug_str = ""
num_debug_icons = len(debug_icons)

column_num = num_debug_icons / 4

debug_str += "\tImGui::Columns(4);\n\n"

column_counter = 0
cur_column = 0
for i in range(0, num_debug_icons):
    if column_counter == column_num and cur_column < 3:
        column_counter = 0
        cur_column += 1
        debug_str += "\tImGui::NextColumn();\n\n"

    column_counter += 1

    debug_str += debug_icons[i]


debug_str += "\tImGui::Columns(1);\n"

print(debug_str)





