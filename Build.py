import os
import shutil
import multiprocessing



if os.name == "posix":
    print("Select the build type:")
    print("\t0 - Debug")
    print("\t1 - Release")
    option = int(input("==> "))
    buildType = ""
    if option == 0:
        buildType = "Debug"
    else:
        buildType = "Release"
    print("Making a "+buildType+" build...")
    os.system("cmake -S . -B build/"+buildType+" -D MAIN_TARGET=DommusHub -D CMAKE_BUILD_TYPE="+buildType+" -D CMAKE_EXPORT_COMPILE_COMMANDS=1 -D CMAKE_RUNTIME_OUTPUT_DIRECTORY=bin")
    try:
        numberOfCores = multiprocessing.cpu_count();
        os.system("cmake --build build/"+buildType+" -j "+str(numberOfCores))
    except:
        print("Fail to get the number of CPU cores. Omitting the jobs flag!")
        os.system("cmake --build build/"+buildType)
    if os.path.exists("build/compile_commands.json"):
        os.remove("build/compile_commands.json")
    shutil.move("build/"+buildType+"/compile_commands.json", "build")
    print(buildType+" build completed!")
elif os.name == "nt":
    os.system("cmake -S . -B build -D MAIN_TARGET=DommusHub -D CMAKE_RUNTIME_OUTPUT_DIRECTORY=bin -G \"Visual Studio 17 2022\"")
else:
    print(f"Platform no supported")
