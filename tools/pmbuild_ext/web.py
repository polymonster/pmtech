import sys
import math


# generates image grid for webgl samples
def website_generator(file):
    lua = open(file).read()
    lines = lua.split("\n")
    apps = []
    for l in lines:
        if l.startswith("create_app_example"):
            if l.find("-- hide") != -1:
                continue
            start = l.find('"')+1
            end = l.find('"', start)
            apps.append(l[start:end])
    rows = int(math.ceil(len(apps)/4))
    cols = [[], [], [], []]
    for r in range(0, rows):
        for c in range(0, 4):
            index = r*4+c
            if index >= len(apps):
                continue
            cols[c].append(apps[r*4+c])
    item_code = '    <a href="http://www.polymonster.co.uk/pmtech/examples/${app}.html"><img src="/images/pmtech/thumbs/${app}.jpg"></a>\n'
    code = '<div class="row">\n'
    for c in cols:
        code += '  <div class="column">\n'
        for app in c:
            code += item_code.replace("${app}", app)
        code += '  </div>\n'
    code += '</div>\n'
    print(code)


if __name__ == "__main__":
    website_generator(sys.argv[1])
