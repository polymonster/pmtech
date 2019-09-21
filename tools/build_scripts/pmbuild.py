import collections
import sys
import jsn.jsn as jsn


# premake
def run_premake(config):
    print(config)
    pass


# pmfx
def run_pmfx(config):
    print(config)
    pass


# models
def run_models(config):
    print(config)
    pass


# textures
def run_textures(config):
    print(config)
    pass


# copy
def run_copy(config):
    print(config)
    pass


# entry point of pmbuild
if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmbuild (v3) -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    config_all = jsn.loads(open("config.jsn", "r").read())

    # tasks are executed in order they are declared
    tasks = collections.OrderedDict()
    tasks["premake"] = run_premake
    tasks["pmfx"] = run_pmfx
    tasks["models"] = run_models
    tasks["textures"] = run_textures
    tasks["copy"] = run_copy

    # first arg is build profile
    config = config_all[sys.argv[1]]

    # run tasks
    for key in tasks.keys():
        if key not in config.keys():
            continue
        if "-all" in sys.argv and "-n" + key not in sys.argv:
            tasks.get(key, lambda config: '')(config)
        elif len(sys.argv) != 2 and "-" + key in sys.argv:
            tasks.get(key, lambda config: '')(config)
        elif len(sys.argv) == 2:
            tasks.get(key, lambda config: '')(config)
