import random

Import("env")

f = open(env.GetProjectOption("custom_version_file"), "w")
f.write("#define VERSION " + str(random.randint(0, 2**32-1)) + "\r") # generate random int32
f.close()