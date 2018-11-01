import sys
import os


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


# write function stub from decl
def write_function_stub(scope, file_pos, file_data):
    delimiters = [';', '{', '}', ':']
    ns = len(file_data)
    ps = -1

    # check for function pointers and bail
    for i in range(file_pos+1, len(file_data)):
        if file_data[i] != " ":
            if file_data[i] == "*":
                return
            else:
                break

    found = False
    i = file_pos
    while not found:
        for d in delimiters:
            if file_data[i] == d:
                ps = i+1
                found = True
                break
        i -= 1

    # skip over functions which have a body
    nns = min(file_pos + file_data[file_pos:].find(";") + 1, ns)
    if file_pos + file_data[file_pos:].find("{") < nns:
        if file_pos + file_data[file_pos:].find("}") < nns:
            return

    ns = min(file_pos + file_data[file_pos:].find(")") + 1, ns)
    func = file_data[ps:ns]
    func = func.strip(" ")

    # properties ie.. const or pure virtual
    es = ns + file_data[ns:].find(";")
    prop = file_data[ns:es]
    prop = prop.strip(" ")

    # dont add pure virtual
    if prop.find("=") != -1:
        prop = ""

    # variable initialisers contains brackets and are not functions
    if func.find("=") != -1:
        if func.find("=") < func.find("("):
            return

    pos = 0
    for char in func:
        if char == '':
            del func[pos]
        pos += 1

    func = func.replace("\n", " ")
    func = func.strip(" ")

    if func == "":
        return

    # args
    args_start = func.find("(") + 1
    args_end = func.find(")")
    args = func[args_start:args_end].split(",")

    # return type and name
    rt_name = func[:args_start-1]
    rt_name.replace("\t", " ")
    rt_name.replace("\n", " ")
    rt_name = rt_name.split(" ")

    name_pos = len(rt_name) - 1
    for r in reversed(rt_name):
        if r != "":
            del rt_name[name_pos]
            name = r
            break
        name_pos -= 1

    # ignore inline function defs
    for r in rt_name:
        if r == "inline":
            return

    for i in range(0, len(args)):
        args[i] = args[i].strip(" ")

    definition = ""

    # return type
    pointer = False
    void = False
    for r in rt_name:
        if r == "void" != -1:
            void = True
        if r.find("*") != -1:
            pointer = True
        if r == '':
            continue
        definition += r
        definition += " "

    # class / struct scope for member function
    for s in scope:
        if len(s) > 0:
            if s[0] == "class" or s[0] == "struct":
                definition += s[1] + "::"

    # name
    definition += name
    definition += "("

    # args
    argnum = 0
    for a in args:
        if argnum > 0:
            definition += ", "
        definition += a
        argnum += 1
    definition += ")"

    # properties
    definition += " " + prop
    definition += "\n"

    # body
    definition += "{\n"

    if pointer:
        definition += "    return nullptr;"
    elif not void:
        definition += "    return "
        for r in rt_name:
            if r != "const":
                definition += r
        definition += "();"

    definition += "\n"
    definition += "}\n"

    return definition


# remove cpp comments
def remove_comments(file_data):
    lines = file_data.split("\n")
    inside_block = False
    conditioned = ""
    for line in lines:
        if inside_block:
            ecpos = line.find("*/")
            if ecpos != -1:
                inside_block = False
                line = line[ecpos+2:]
            else:
                continue
        cpos = line.find("//")
        mcpos = line.find("/*")
        if cpos != -1:
            conditioned += line[:cpos] + "\n"
        elif mcpos != -1:
            conditioned += line[:mcpos] + "\n"
            inside_block = True
        else:
            conditioned += line + "\n"

    return conditioned


def indent_str(count):
    istr = ""
    for i in range(count):
        istr += "    "
    return istr


def generate_stub_functions(file_data, filename):
    file_data = tabs_to_spaces(file_data, 4)
    file_data = remove_comments(file_data)
    lines = file_data.split("\n")
    scope = []
    output = "#include " + filename + "\n\n"
    indent = 0
    for l in lines:
        tokens = l.split(" ")
        bpos = l.find("(")
        if bpos != -1:
            file_pos = file_data.find(l)
            f = write_function_stub(scope, file_pos + bpos, file_data)
            if f:
                fl = f.split("\n")
                for l in fl:
                    output += indent_str(indent)
                    output += l + "\n"
        for i in range(0, len(tokens)):
            t = tokens[i]
            if t[0:2] == "//":
                break
            if t == "namespace" or t == "class" or t == "struct":
                if i + 1 < len(tokens):
                    qualifier = (t, tokens[i+1])
            if t.find("{") != -1:
                if len(qualifier) > 0:
                    if qualifier[0] == "namespace":
                        output += indent_str(indent) + qualifier[0] + " " + qualifier[1] + "\n"
                        output += indent_str(indent) + "{\n\n"
                        indent += 1
                scope.append(qualifier)
                qualifier = ()
            if t.find("}") != -1:
                close_qualifier = scope.pop()
                if len(close_qualifier) > 0:
                    if close_qualifier[0] == "namespace":
                        indent -= 1
                        output += indent_str(indent) + "}\n\n"

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
                file_data = generate_stub_functions(file_data, os.path.basename(input_file))
                print(file_data)

            if 0:
                file = open(input_file, "w")
                file.write(file_data)
                file.close()
