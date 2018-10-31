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


def identify_function(scope, file_pos, file_data):
    delimiters = [';', '{', '}']
    ns = len(file_data)
    ps = -1
    found = False
    i = file_pos
    while found != True:
        for d in delimiters:
            if file_data[i] == d:
                ps = i+1
                found = True
                break
        i -= 1
    ns = min(file_pos + file_data[file_pos:].find(")") + 1, ns)
    func = file_data[ps:ns]
    func = func.strip(" ")

    # skip variable initialisers
    if func.find("=") != -1:
        if func.find("=") < func.find("("):
            return

    # strip comments
    cps = func.find("//")
    if cps != -1:
        func = func[func.find("\n"):]

    cps = func.find("/*")
    if cps != -1:
        func = func[func.find("*/"):]

    func = func.replace("\t", " ")
    func = func.replace("\n", " ")
    func = func.strip(" ")

    args_start = func.find("(") + 1
    args_end = func.find(")")
    args = func[args_start:args_end].split(",")

    rt_name = func[:args_start-1]
    rt_name.replace("\t", " ")
    rt_name.replace("\n", " ")
    rt_name = rt_name.split(" ")

    for r in reversed(rt_name):
        if r != "":
            name = r
            break

    print(scope)
    print(name)
    print(args)


def generate_stub_functions(file_data):
    lines = file_data.split("\n")
    scope = []
    output = ""
    file_pos = 0
    for l in lines:
        tokens = l.split(" ")
        bpos = l.find("(")
        if bpos != -1:
            identify_function(scope, file_pos + bpos, file_data)
        for i in range(0, len(tokens)):
            t = tokens[i]
            if t[0:2] == "//":
                break
            if t == "namespace" or t == "class" or t == "struct":
                if i + 1 < len(tokens):
                    qualifier = (t, tokens[i+1])
            if t.find("{") != -1:
                scope.append(qualifier)
                qualifier = ()
            if t.find("}") != -1:
                scope.pop()
        file_pos += len(l)

    return output


if __name__ == "__main__":
    if len(sys.argv) <= 1:
        display_help()
    else:
        input_files = []
        if "-i" in sys.argv:
            pos = sys.argv.index("-i") + 1
            while pos < len(sys.argv) and sys.argv[pos][0] != "-":
                input_files.append(sys.argv[pos])
                pos += 1
        for input_file in input_files:
            file = open(input_file, "r")
            file_data = file.read()
            file.close()

            if "-tabs_to_spaces" in sys.argv:
                spaces = sys.argv[sys.argv.index("-tabs_to_spaces") + 1]
                file_data = tabs_to_spaces(file_data, int(spaces))

            if "-align_consecutive" in sys.argv:
                align_char = sys.argv[sys.argv.index("-align_consecutive") + 1]
                file_data = align_consecutive(file_data, align_char)

            if "-generate_stub_functions" in sys.argv:
                file_data = generate_stub_functions(file_data)

            if 0:
                file = open(input_file, "w")
                file.write(file_data)
                file.close()
