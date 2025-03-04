#include "yaml-cpp/yaml.h"

#ifdef __linux__
#include <bits/types/FILE.h>
#elif _WIN32
#include <shtypes.h>
#include <shobjidl_core.h>
#include <stringapiset.h>
#endif
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iterator>
#include <string>
#include <thread>
#include <assert.h>
#include <cstring>



#define INFOS_FILE_NAME "Infos.yaml"
#define PROJECTS "Projects"
#define PROJECT_NAME "Name"
#define PROJECT_PATH "ProjectPath"
#define EDITOR_PATH "EditorPath"
#define PATH_SIZE 1024
#define CMAKE_LISTS "CMakeLists.txt"
#define SET_EDITOR_PATH_OPTION 1
#define CREATE_PROJECT_OPTION 2
#define SELECT_PROJECT_OPTION 3
#define OPEN_PROJECT_OPTION 4
#define BUILD_PROJECT_OPTION 5
#define EXIT_OPTION 6

static constexpr size_t s_numberOfFmodLibsPaths(3);
static const char* s_fmodLibsPaths[s_numberOfFmodLibsPaths]{
	"src/Core/vendor/fmodstudio/api/core/lib/x86_64/libfmod.so",
	"src/Core/vendor/fmodstudio/api/core/lib/x86_64/libfmod.so.13",
	"src/Core/vendor/fmodstudio/api/core/lib/x86_64/libfmod.so.13.26"
};

void CreateInfoFile()
{
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	{
		emitter << YAML::Key << EDITOR_PATH << YAML::Value << "";
		emitter << YAML::Key << PROJECTS << YAML::Value << YAML::BeginMap;
		{
			emitter << YAML::EndMap;
		}
		emitter << YAML::EndMap;
	}
	std::ofstream fileOutStream;
	fileOutStream.open(INFOS_FILE_NAME, std::ios_base::out);
	fileOutStream << emitter.c_str();
	fileOutStream.close();
}

YAML::Node OpenInfosFile()
{
	YAML::Node infosNode;
	try
	{
		infosNode = YAML::LoadFile(INFOS_FILE_NAME);
	}
	catch (const std::exception& e)
	{
		CreateInfoFile();
		infosNode = YAML::LoadFile(INFOS_FILE_NAME);
	}
	return infosNode;
}

bool GetDirectoryPath(char* buf, size_t size, const char* fileBrowserTitle)
{
	std::memset(buf, '\0', size);
#ifdef __linux__
	char command[PATH_SIZE];
	std::strcpy(command, "zenity --file-selection --directory --title=");
	std::strcat(command, fileBrowserTitle);
	FILE* f(popen(command, "r"));
	if (f == nullptr)
	{
		std::cout << strerror(errno) << std::endl;
		return false;
	}
	std::fgets(buf, size, f);
	int status(pclose(f));
	if (status == -1)
	{
		std::cout << strerror(errno) << std::endl;
		return false;
	}
	if (strlen(buf) == 0)
	{
		return false;
	}
	for (size_t offset(0); offset < size; offset++)
	{
		if (buf[offset] == '\n')
		{
			buf[offset] = '\0';
		}
	}
	return true;
#elif _WIN32
	bool result(false);
	IFileDialog* pfd = NULL;
	HRESULT hr = CoCreateInstance(
		CLSID_FileOpenDialog,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pfd));
	if (SUCCEEDED(hr))
	{
		if (SUCCEEDED(hr))
		{
			DWORD dwFlags;
			hr = pfd->GetOptions(&dwFlags);
			if (SUCCEEDED(hr))
			{
				wchar_t title[PATH_SIZE];
				std::mbstowcs(title, fileBrowserTitle, PATH_SIZE);
				hr = pfd->SetTitle(title);
				if (SUCCEEDED(hr))
				{
					hr = pfd->SetOptions(dwFlags | FOS_PICKFOLDERS);
					if (SUCCEEDED(hr))
					{
						hr = pfd->Show(NULL);
						if (SUCCEEDED(hr))
						{
							IShellItem* psiResult;
							hr = pfd->GetResult(&psiResult);
							if (SUCCEEDED(hr))
							{
								PWSTR pszFilePath = NULL;
								hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
								if (SUCCEEDED(hr))
								{
									int conversionResult(
										WideCharToMultiByte(
											CP_ACP,
											0,
											pszFilePath,
											std::wcslen(pszFilePath),
											buf,
											size,
											nullptr,
											nullptr
										)
									);
									if (conversionResult == 0)
									{
										std::cout << "Fail to get path: wide characters used." << std::endl;
									}
									else
									{
										result = true;
									}
									CoTaskMemFree(pszFilePath);
								}
								psiResult->Release();
							}
						}
					}
				}
			}
		}
		pfd->Release();
	}
	return result;
#endif
}

void SetEditorPath()
{
	char buf[PATH_SIZE];
	if (!GetDirectoryPath(buf, PATH_SIZE, "Select the editor directory"))
	{
		return;
	}
 	YAML::Node infos(YAML::LoadFile(INFOS_FILE_NAME));
	YAML::Node editorPathNode(infos[EDITOR_PATH]);
	if (!editorPathNode)
	{
		return;
	}
	infos[EDITOR_PATH] = buf;
	std::ofstream fout(INFOS_FILE_NAME, std::ios_base::out | std::ios_base::trunc);
	if (!fout.good())
	{
		std::cout << "Fail to open infos file." << std::endl;
		fout.close();
		return;
	}
	fout << infos;
	fout.close();
	std::cout << "Editor path set to: " << buf << std::endl; 
}

void CreateProject()
{
	char projectName[PATH_SIZE];
	char projectPath[PATH_SIZE];
	std::memset(projectName, '\0', PATH_SIZE);
	std::memset(projectPath, '\0', PATH_SIZE);
	std::cout << "Enter project name: ";
	std::cin >> projectName;
	YAML::Node infos(YAML::LoadFile(INFOS_FILE_NAME));
	YAML::Node projectsNode(infos[PROJECTS]);
	if (projectsNode[projectName])
	{
		std::cout << "Project with name " << projectName << " alredy exists." << std::endl;
		return;
	}
	if (!GetDirectoryPath(projectPath, PATH_SIZE, "\"Select project directory\""))
	{
		return;
	}
	projectsNode[projectName] = projectPath;
	std::ofstream fout(INFOS_FILE_NAME, std::ios_base::out | std::ios_base::trunc);
	if (!fout.good())
	{
		std::cout << "Fail to open infos file." << std::endl;
		fout.close();
		return;
	}
	fout << infos;
	fout.close();
	std::filesystem::path assetsPath(projectPath);
	std::filesystem::path cmakePath(projectPath);
	assetsPath /= "assets";
	cmakePath /= "CMakeLists.txt";
	std::filesystem::path animationsPath(assetsPath);
	std::filesystem::path animationStateMachinesPath(assetsPath);
	std::filesystem::path materialsPath(assetsPath);
	std::filesystem::path physicsMaterialsPath(assetsPath);
	std::filesystem::path scenesPath(assetsPath);
	std::filesystem::path texturesPath(assetsPath);
	const std::filesystem::path spriteSheetGenPath(std::filesystem::path(assetsPath) / "sprite sheet gen");
	animationsPath /= "animation";
	animationStateMachinesPath /= "animation state machine";
	materialsPath /= "material";
	physicsMaterialsPath /= "physics material";
	texturesPath /= "texture";
	scenesPath /= "scene";
	std::ofstream projectCmakeFile(cmakePath.c_str());
	if (!projectCmakeFile.good())
	{
		std::cout << "Fail to create cmake file in project directory." << std::endl;
		projectCmakeFile.close();
		return;
	}
	projectCmakeFile << "add_subdirectory(assets)";
	projectCmakeFile.close();
	bool assetsDirectoryStatus(std::filesystem::create_directory(assetsPath));
	bool animationsDirectoryStatus(std::filesystem::create_directory(animationsPath));
	bool animationStateMachinesDirectoryStatus(std::filesystem::create_directory(animationStateMachinesPath));
	bool materialsDirectoryStatus(std::filesystem::create_directory(materialsPath));
	bool physicsMaterialsDirectoryStatus(std::filesystem::create_directory(physicsMaterialsPath));
	bool texturesDirectoryStatus(std::filesystem::create_directory(texturesPath));
	bool scenesDirectoryStatus(std::filesystem::create_directory(scenesPath));
	bool spriteSheetGenStatus(std::filesystem::create_directory(spriteSheetGenPath));
	if (!assetsDirectoryStatus || 
		!animationsDirectoryStatus ||
		!animationStateMachinesDirectoryStatus ||
		!materialsDirectoryStatus ||
		!physicsMaterialsDirectoryStatus ||
		!texturesDirectoryStatus || 
		!scenesDirectoryStatus ||
		!spriteSheetGenStatus)
	{
		std::cout << "Fail to create project." << std::endl;
		return;
	}
	std::filesystem::path runPath(projectPath);
	runPath /= "Run.py";
	std::ofstream runFile(runPath);
	runFile << "import os\n\n\n";
	runFile << "os.system(\"" << "./DommusEditor " << assetsPath.string() << " " << infos[EDITOR_PATH].as<std::string>() << "/assets" << "\")";
	if (!runFile.good())
	{
		std::cout << "Fail to create Run file" << std::endl;
		runFile.close();
		return;
	}
	runFile.close();
	assetsPath /= "CMakeLists.txt";
	std::ofstream assetsCmakeFile(assetsPath.c_str());
	if (!assetsCmakeFile.good())
	{
		std::cout << "Fail to create assets CMakeLists.txt file." << std::endl;
		assetsCmakeFile.close();
		return;
	}
	assetsCmakeFile.close();
	std::cout << "Project " << projectName << " created with success!\n" << std::endl;
}

void SelectProject(std::string& selectedProject)
{
	YAML::Node infos(YAML::LoadFile(INFOS_FILE_NAME));
	if (!infos || infos[PROJECTS].size() == 0)
	{
		std::cout << "No project created.\n" << std::endl;
		return;
	}
	while (true)
	{
		int option;
		std::cout << "Select a project:" << std::endl;
		YAML::Node projectsNode(infos[PROJECTS]);
		size_t currentOption(0);
		for (YAML::const_iterator it(projectsNode.begin()); it != projectsNode.end(); it++)
		{
			std::cout << currentOption << " - " << it->first.as<std::string>() << std::endl;
			currentOption++;
		}
		std::cout << "\n==> ";
		std::cin >> option;
		if(option < 0 || option >= projectsNode.size())
		{
			std::cout << "Invalid value!" << std::endl;
			continue;
		}
		YAML::const_iterator projectIterator(projectsNode.begin());
		std::advance(projectIterator, option);
		selectedProject = projectIterator->first.as<std::string>();
		break;
	}
}

void OpenProject()
{
	char projectPath[PATH_SIZE];
	char projectName[PATH_SIZE];
	std::cout << "Enter project name: ";
	std::cin >> projectName;
	YAML::Node infos(OpenInfosFile());
	assert(infos[PROJECTS]);
	if (infos[PROJECTS][projectName])
	{
		std::cout << "Fail to open project: project with name " << projectName << " is already added." << std::endl;
		return;
	}
	if (!GetDirectoryPath(projectPath, PATH_SIZE, "Select project root directory"))
	{
		return;
	}
	infos[PROJECTS].force_insert(projectName, projectPath);
	std::ofstream stream(INFOS_FILE_NAME, std::ios_base::out, std::ios_base::trunc);
	assert(stream);
	stream << infos;
	assert(stream);
	stream.close();
	assert(stream);
}

void BuildProject(const std::string& selectedProject)
{
	if (selectedProject.empty())
	{
		std::cout << "No project selected.\n" << std::endl;
		return;
	}
	YAML::Node infos(YAML::LoadFile(INFOS_FILE_NAME));
	if (!infos || !infos[EDITOR_PATH] || !infos[PROJECTS][selectedProject])
	{
		std::cout << "Fail to open infos file.\n" << std::endl;
		return;
	}
	std::filesystem::path editorPath(infos[EDITOR_PATH].as<std::string>());
	if (editorPath.empty())
	{
		std::cout << "Editor path not selected.\n" << std::endl;
		return;
	}
	int option(-1);
	while (option != 0 && option != 1)
	{
		std::cout << "Select build type:" << std::endl;
		std::cout << "0 - Debug" << std::endl;
		std::cout << "1 - Release" << std::endl;
		std::cout << "\n==> ";
		std::cin >> option;
		if (option != 0 && option != 1)
		{
			std::cout << "Invalid option.\n" << std::endl;
		}
	}
	std::filesystem::path projectPath(infos[PROJECTS][selectedProject].as<std::string>());
	std::filesystem::path runtimeOutputDirectory(projectPath);
	std::string command("cmake");
	command.append(" -S \"").append(editorPath.string()).append("\"").append(" -B \"").append((projectPath / "build").string()).append("\"");
	command.append(" -D MAIN_TARGET=DommusEditor");
	command.append(" -D PROJECT_PATH_DEFINED=ON");
	command.append(" -D PROJECT_PATH=\"").append(projectPath.string()).append("\"");
	command.append(" -D CMAKE_BUILD_TYPE=").append(option == 0 ? "Debug" : "Release");
	command.append(" -D CMAKE_EXPORT_COMPILE_COMMANDS=ON");
	command.append(" -D CMAKE_RUNTIME_OUTPUT_DIRECTORY=\"").append(runtimeOutputDirectory.string()).append("\"");
	int status(system(command.c_str()));
	if (status != 0)
	{
		std::cout << "Fail to generate debug build files" << selectedProject << "." << std::endl;
		return;
	}
	command = "cmake --build ";
	command.append("\"").append(projectPath.string()).append("/build").append("\"");
	command.append(" -j ").append(std::to_string(std::thread::hardware_concurrency()));
	status = system(command.c_str());
	if (status != 0)
	{
		std::cout << "Fail to build project " << selectedProject << "." << std::endl;
		return;
	}
	std::cout << "Project " << selectedProject << " built with success! (Mode: " << (option == 0 ? "debug" : "release") << ")\n" << std::endl;
}

void GenerateSolution(const std::string& selectedProject)
{
	if (selectedProject.empty())
	{
		std::cout << "No project selected.\n" << std::endl;
		return;
	}
	YAML::Node infos(YAML::LoadFile(INFOS_FILE_NAME));
	if (!infos || !infos[EDITOR_PATH] || !infos[PROJECTS][selectedProject])
	{
		std::cout << "Fail to open infos file.\n" << std::endl;
		return;
	}
	std::filesystem::path editorPath(infos[EDITOR_PATH].as<std::string>());
	if (editorPath.empty())
	{
		std::cout << "Editor path not selected.\n" << std::endl;
		return;
	}
	std::filesystem::path projectPath(infos[PROJECTS][selectedProject].as<std::string>());
	std::filesystem::path runtimeOutputDirectory(projectPath);
	std::string command("cmake");
	const char* assetsDirectoryName("assets");
	command.append(" -S \"").append(editorPath.string()).append("\"").append(" -B \"").append((projectPath / "build").string()).append("\"");
	command.append(" -D MAIN_TARGET=DommusEditor");
	command.append(" -D PROJECT_PATH_DEFINED=ON");
	command.append(" -D PROJECT_PATH=\"").append(projectPath.string()).append("\"");
	command.append(" -D EDITOR_PATH=\"").append(editorPath.string()).append("\"");
	command.append(" -D CMAKE_BUILD_TYPE=NOT_DEFINED");
	command.append(" -D CMAKE_VS_DEBUGGER_COMMAND_ARGUMENTS=\"").append((projectPath / assetsDirectoryName).string()).append(" ").append((editorPath / assetsDirectoryName).string()).append("\"");
	command.append(" -D CMAKE_RUNTIME_OUTPUT_DIRECTORY=\"").append(runtimeOutputDirectory.string()).append("\"");
	int status(system(command.c_str()));
	if (status != 0)
	{
		std::cout << "Fail to generate debug build files for " << selectedProject << "." << std::endl;
		return;
	}
}

int main(void)
{
#ifdef _WIN32
	assert(SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)));
#endif
	OpenInfosFile();
	int option(0);
	std::string selectedProject;
	while (option != EXIT_OPTION)
	{
		std::cout << "Selected project: " << (selectedProject.size() == 0 ? "NONE" : selectedProject) << "\n" << std::endl;
		std::cout << SET_EDITOR_PATH_OPTION << " - Set editor path." << std::endl;
		std::cout << CREATE_PROJECT_OPTION << " - Create project." << std::endl;
		std::cout << SELECT_PROJECT_OPTION << " - Select project." << std::endl;
		std::cout << OPEN_PROJECT_OPTION << " - Open project." << std::endl;
#ifdef __linux__
		std::cout << BUILD_PROJECT_OPTION << " - Build project." << std::endl;
#elif _WIN32
		std::cout << BUILD_PROJECT_OPTION << " - Generate Visual Studio solution" << std::endl;
#endif
		std::cout << EXIT_OPTION << " - Exit." << std::endl << std::endl;
		std::cout << "==> ";
		std::cin >> option;
		switch (option) 
		{
		case SET_EDITOR_PATH_OPTION:
			SetEditorPath();
			continue;
		case CREATE_PROJECT_OPTION:
			CreateProject();
			continue;
		case SELECT_PROJECT_OPTION:
			SelectProject(selectedProject);	
			continue;
		case OPEN_PROJECT_OPTION:
			OpenProject();
			continue;
		case BUILD_PROJECT_OPTION:
#ifdef __linux__
			BuildProject(selectedProject);
#elif _WIN32
			GenerateSolution(selectedProject);
#endif
			continue;
		case EXIT_OPTION:
			continue;
		default:
			std::cout << "Invalid option.\n" << std::endl;
			continue;
		}
	}
	return EXIT_SUCCESS;
}
