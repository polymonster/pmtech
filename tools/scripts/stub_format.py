import sys
import os
import cgu


def display_help():
    print("stub format help:")
    print("    -help (display this dialog)")
    print("    -i <input file>")
    print("    -o <output file>")
    print("    -w <overwrite input file in place>")
    print("    -p (print result to console)")
    print("    -stub (generate function stubs from input file")
    print("    -align <char> (align consecutive lines by char)")
    print("    -tabs <num spaces per tab> (replace tabs with spaces)")
    print("    -rm_comments (remove comments)")
    print("    -camel_to_snake (convert camel case to snake case)")
    print("    -snake_to_camel (convert snake case to camel case)")
    print("    -disclaimer (include contents of disclaimer.h at the top of source files)")
    print("    -pragma_once (replace ifndef header guard with pragma once)")
    print("    -test_gen (generates code to print input args and return value, for generating test data)")


# indent with spaces
def indent_str(count):
    istr = ""
    for i in range(count):
        istr += "    "
    return istr


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


# find the end of a body text enclosed in brackets
def enclose_brackets(text):
    body_pos = text.find("{")
    bracket_stack = ["{"]
    text_len = len(text)
    while len(bracket_stack) > 0 and body_pos < text_len:
        body_pos += 1
        character = text[body_pos:body_pos+1]
        if character == "{":
            bracket_stack.insert(0, "{")
        if character == "}" and bracket_stack[0] == "{":
            bracket_stack.pop(0)
            body_pos += 1
    return body_pos


def add_line(line, output):
    return output + line + "\n"

def add_line_test(line, output):
    return output + "if(_test_stack_depth == 1)" + line + "\n"

# inject prints into source for test gen
def inject_function_test_gen(file_pos, file_data, output):
    ns = len(file_data)
    nns = min(file_pos + file_data[file_pos:].find(";") + 1, ns)
    body_start = file_pos + file_data[file_pos:].find("{")
    be = 0
    if body_start < nns:
        args_start = file_pos + file_data[file_pos:].find("(")
        args_end = args_start + file_data[args_start:].find(")")
        be = enclose_brackets(file_data[file_pos:])

        # analyse function source
        prototype = file_data[file_pos:body_start].strip()
        body = file_data[body_start:file_pos+be].strip(" {}")
        args = file_data[args_start:args_end].strip(" ()").split(",")
        fn_name = file_data[:args_start].rfind(" ")
        rt = file_data[file_pos:fn_name].strip().replace("inline ", "")
        fn_name = file_data[fn_name:args_start].strip()

        output = add_line(prototype, output)
        output = add_line("{", output)
        output = add_line("_test_stack_depth++;", output)

        output = add_line_test("std::cout << \"{\\n\";", output)
        output = add_line_test("std::cout << \"    //" + fn_name + "---------------------------\\n\";", output)

        pass_args = ""
        check_args = []
        for a in args:
            a = a.strip()
            is_const = a.find("const") != -1
            is_ref = a.find("&") != -1
            is_ptr = a.find("*") != -1
            is_float = a.find("f32 ") != -1 or a.find("float ") != -1
            cast = ""
            if is_float:
                cast = "(f32)"
            a = a.replace("&", "")
            a = a.replace("*", "")
            al = a.split(" ")
            name = al[len(al)-1]
            if not is_const and is_ref:
                output = add_line_test("std::cout << " + "\"    " + str(a) + " = { 0 };\\n\";", output)
                check_args.append(name)
            elif is_ptr:
                output = add_line_test("std::cout << " + "\"    " + str(a) + ";\\n\";", output)
                check_args.append(name)
            else:
                output = add_line_test("std::cout << " + "\"    " + str(a) + " = {" + cast + "\" << " + name + " << \"};\\n\";", output)
            if len(pass_args) > 0:
                pass_args += ", "
            if is_ptr:
                pass_args += "&"
            pass_args += str(name)

        output = add_line_test("std::cout << \"    " + rt + " result = " + fn_name + "(" + pass_args + ");\\n\";", output)

        rp = 0
        while True:
            op = rp
            rp = body[rp:].find("return ")
            if rp == -1:
                break;
            rp = op + rp
            sc = rp + body[rp:].find(";")
            rv = body[rp:sc].replace("return ", "")
            output = add_line(body[op:rp], output)
            output = add_line("{", output)
            output = add_line_test("std::cout << \"    REQUIRE(require_func(result," + rt + "(\" << (" + rv + ") << " + "\")));\\n\";", output)
            for ca in check_args:
                output = add_line_test("std::cout << \"    REQUIRE(require_func(" + ca + ",{\" << (" + ca + ") << " "\"}));\\n\";", output)
            output = add_line_test("std::cout << \"}\\n\";", output)
            output = add_line("\n_test_stack_depth--;", output)
            output = add_line(body[rp:sc] + ";", output)
            output = add_line("}", output)
            rp = sc+1
        output = add_line(body[op:], output)
        output = add_line("}", output)
    return be, output


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
    # add ovveride for virtual
    override = False
    for r in rt_name:
        if r == "inline":
            return
        if r == "virtual":
            override = True

    for i in range(0, len(args)):
        args[i] = args[i].strip(" ")

    definition = ""

    # return type
    pointer = False
    void = False
    for r in rt_name:
        if r == "static":
            continue
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
        default = a.find("=")
        if default != -1:
            a = a[:default]
            a = a.strip(" ")
        if argnum > 0:
            definition += ", "
        definition += a
        argnum += 1
    definition += ")"

    # properties
    definition += " " + prop
    if override:
        definition += " override "

    definition += "\n"

    # body
    definition += "{\n"

    if pointer:
        definition += "    return nullptr;"
    elif not void and len(rt_name) > 0:
        definition += "    return "
        for r in rt_name:
            if r != "const" and r != "virtual":
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


# create stub c/c++ functions
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


def prev_delim(text):
    delims = ["\n", ";", "}"]
    v = len(text)+1
    for d in delims:
        vv = text.rfind(d)
        if vv < v and vv != -1:
            v = vv
    return v


# create c/c++ test
def generate_cpp_test(file_data, filename):
    file_data = tabs_to_spaces(file_data, 4)
    file_data = remove_comments(file_data)
    file_pos = 0
    output = ""
    while True:
        bpos = file_data[file_pos:].find("(")
        if bpos != -1:
            line_pos = file_pos + file_data[file_pos:file_pos+bpos].rfind("\n")
            offset, output = inject_function_test_gen(line_pos, file_data, output)
            file_pos += offset
            if offset == 0:
                file_pos = file_pos+bpos+1

        else:
            break
    # format output
    fmt = ""
    lines = output.split("\n")
    for l in lines:
        l = l.strip()
        if l == "":
            continue
        fmt += l + "\n"
    print(fmt)
    return file_data


# snake_case to CamelCase
def snake_to_camel(file_data):
    num_chars = len(file_data)
    out_data = ""
    for i in range(num_chars):
        if file_data[i] == "_":
            continue
        if i > 0:
            if file_data[i-1] == "_":
                out_data += file_data[i].upper()
                continue
        out_data += file_data[i]
    return out_data


# CamelCase to snake_case
def camel_to_snake(file_data):
    num_chars = len(file_data)
    out_data = ""
    for i in range(num_chars):
        if file_data[i].isupper():
            if i > 0:
                if file_data[i-1].islower():
                    out_data += "_" + file_data[i].lower()
                    continue
            out_data += file_data[i].lower()
        else:
            out_data += file_data[i]
    out_data = cgu.replace_token("true", "True", out_data)
    out_data = cgu.replace_token("false", "False", out_data)
    out_data = cgu.replace_token("none", "None", out_data)
    out_data = cgu.replace_token("zipfile.zip_file", "zipfile.ZipFile", out_data)
    out_data = cgu.replace_token("zipfile.zip_deflated", "zipfile.ZIP_DEFLATED", out_data)
    out_data = cgu.replace_token("subprocess.popen", "subprocess.Popen", out_data)
    out_data = cgu.replace_token("subprocess.pipe", "subprocess.PIPE", out_data)
    out_data = cgu.replace_token("xml.etree.element_tree", "xml.etree.ElementTree", out_data)
    out_data = cgu.replace_token("collections.ordered_dict", "collections.OrderedDict", out_data)

    return out_data


# add a copyright disclaimer to file
def insert_disclaimer(file_data, filename):
    disclaimer_template = open("disclaimer.h", "r")
    disclaimer_template = disclaimer_template.read()
    disclaimer_template = disclaimer_template.replace("<filename>", os.path.basename(filename))
    disclaimer_template += file_data
    return disclaimer_template


# convert header guard to pragma once
def ifndef_to_pragma_once(file_data):
    pos = file_data.find("#ifndef")
    nl = file_data.find("\n", pos)
    guard = file_data[pos: nl]
    file_data = file_data.replace(guard, "#pragma once")
    define = guard.replace("#ifndef", "#define")
    file_data = file_data.replace(define, "")
    endif = file_data.rfind("#endif")
    file_data = file_data[0:endif]
    return file_data


if __name__ == "__main__":
    if len(sys.argv) <= 1:
        display_help()
    elif "-help" in sys.argv:
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

            if "-test_gen" in sys.argv:
                file_data = generate_cpp_test(file_data, os.path.basename(input_file))

            if "-tabs" in sys.argv:
                spaces = sys.argv[sys.argv.index("-tabs") + 1]
                file_data = tabs_to_spaces(file_data, int(spaces))

            if "-rm_comments" in sys.argv:
                file_data = remove_comments(file_data)

            if "-align" in sys.argv:
                align_char = sys.argv[sys.argv.index("-align") + 1]
                file_data = align_consecutive(file_data, align_char)

            if "-stub" in sys.argv:
                file_data = generate_stub_functions(file_data, os.path.basename(input_file))

            if "-camel_to_snake" in sys.argv:
                file_data = camel_to_snake(file_data)

            if "-snake_to_camel" in sys.argv:
                file_data = snake_to_camel(file_data)

            if "-disclaimer" in sys.argv:
                file_data = insert_disclaimer(file_data, input_file)

            if "-pragma_once" in sys.argv:
                file_data = ifndef_to_pragma_once(file_data)

            if "-p" in sys.argv:
                print(file_data)

            if "-w" in sys.argv:
                file = open(input_file, "w")
                file.write(file_data)
                file.close()
