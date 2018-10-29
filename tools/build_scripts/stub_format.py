import sys


def display_help():
    print("stub format help:")


# replace tabs with spaces for consistent display across editors and websites
def tabs_to_spaces(file_data, num_spaces):
    spaces_string = ""
    for i in range(0, num_spaces):
        spaces_string += " "
    return file_data.replace("\t", spaces_string)


# align consecutive lines containing align_char
def align_consecutive(file_data, align_char):
    lines = file_data.split("\n")
    align_lines = []
    alignment_pos = -1
    output = ""
    for line in lines:
        pos = line.find(align_char)
        if pos != -1:
            alignment_pos = max(pos, alignment_pos)
            align_lines.append(line)
        else:
            for align in align_lines:
                pos = align.find(align_char)
                pad = alignment_pos - pos
                string_pad = ""
                for i in range(0, pad):
                    string_pad += " "
                output += align[:pos] + string_pad + align[pos:] + "\n"
            align_lines.clear()
            alignment_pos = -1
            output += line + "\n"
    return output


if __name__ == "__main__":
    if len(sys.argv) <= 1:
        display_help()
    else:
        if "-i" in sys.argv:
            input_file = sys.argv[sys.argv.index("-i") + 1]

        file = open(input_file, "rw")
        file_data = file.read()

        if "-tabs_to_spaces" in sys.argv:
            spaces = sys.argv[sys.argv.index("-tabs_to_spaces") + 1]
            file_data = tabs_to_spaces(file_data, int(spaces))

        if "-align_consecutive" in sys.argv:
            align_char = sys.argv[sys.argv.index("-align_consecutive") + 1]
            file_data = align_consecutive(file_data, align_char)

        file.write(file_data)