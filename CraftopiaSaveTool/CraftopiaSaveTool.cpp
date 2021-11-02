#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include <iostream>
#include <Shlobj_core.h>
#include <locale>
#include <codecvt>
#include <fstream>
#include <filesystem>
#include <stdio.h>
#include <sys/stat.h>
#include <unordered_set>

#include <cli11.hpp>
#include <nlohmann/json.hpp>

// Steam: C:\Users\*\AppData\LocalLow\PocketPair\Craftopia\Save\SaveData.ocs
std::string steamSaveDataPath = "";
void getSteamPath() {
	PWSTR localLowPath;
	SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, NULL, NULL, &localLowPath);

	std::wstring wPath(localLowPath);

	CoTaskMemFree(localLowPath);

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::string path = converter.to_bytes(wPath);
	
	path.append("\\PocketPair\\Craftopia\\Save\\SaveData.ocs");

	//std::clog << path << std::endl;

	std::ifstream f(path.c_str());
	if (f.good()) {
		steamSaveDataPath = path;
	}
}
const bool steamSaveExists() {
	return steamSaveDataPath != "";
}

// UWP: C:\Users\*\AppData\Local\Packages\PocketpairInc.Craftopia_ad4psfrxyesvt\SystemAppData\wgs\000901FAB378ACBB_00000000000000000000000061E454D3\A82E62834A1F4A5BAA5A2CFC1408BC3A\D15B94293BFE43F8B40A5B03EF8E3A2E
std::string uwpSaveDataPath = "";
void getUWPPath() {
	PWSTR localPath;
	SHGetKnownFolderPath(FOLDERID_LocalAppData, NULL, NULL, &localPath);

	std::wstring wPath(localPath);

	CoTaskMemFree(localPath);

	wPath.append(L"\\Packages");

	bool foundPocketPairFolder = false;
	for (const auto & entry : std::filesystem::directory_iterator(wPath)) {
		if (entry.is_directory()) {

			std::wstring entryWStr(entry.path());

			if (entryWStr.find(L"\\Pocketpair") != std::string::npos) {
				wPath = entryWStr;
				foundPocketPairFolder = true;
				break;
			}
		}
	}
	if (!foundPocketPairFolder) return;

	wPath.append(L"\\SystemAppData\\wgs");

	bool foundWGSSubDir = false;
	for (const auto & entry : std::filesystem::directory_iterator(wPath)) {
		if (entry.is_directory()) {
			std::wstring entryWStr(entry.path());
			if (entryWStr.length() - wPath.length() > 4) {
				wPath = entryWStr;
				foundWGSSubDir = true;
				break;
			}
		}
	}
	if (!foundWGSSubDir) return;

	for (const auto & entry : std::filesystem::directory_iterator(wPath)) {
		if (entry.is_directory()) {
			std::wstring entryWStr(entry.path());
			wPath = entryWStr;
		}
	}

	for (const auto & entry : std::filesystem::directory_iterator(wPath)) {
		if (entry.file_size() > 1000) {
			std::wstring entryWStr(entry.path());
			wPath = entryWStr;
		}
	}

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::string path = converter.to_bytes(wPath);

	std::ifstream f(path.c_str());
	if (f.good()) {
		uwpSaveDataPath = path;
	}
}
const bool uwpSaveExists() {
	return uwpSaveDataPath != "";
}

void createBackup() {
	if (steamSaveExists()) {
		std::string backUpFile = steamSaveDataPath;
		backUpFile.append(".backup");
		CopyFileA(steamSaveDataPath.c_str(), backUpFile.c_str(), false);
	}
	if (uwpSaveExists()) {
		std::string backUpFile = uwpSaveDataPath;
		backUpFile.append(".backup");
		CopyFileA(uwpSaveDataPath.c_str(), backUpFile.c_str(), false);
	}
}

std::string externalDecompress(std::string filePath) {
	std::streambuf * old = std::clog.rdbuf(nullptr);
	char buffer[128];
	std::string result = "";
	std::string cmd = "decompress.exe ";
	cmd.append(filePath);
	FILE* pipe = _popen(cmd.c_str(), "r");
	if (!pipe) throw std::runtime_error("popen() failed!");
	try {
		while (fgets(buffer, sizeof buffer, pipe) != NULL)
			result += buffer;
	}
	catch (...) {
		_pclose(pipe);
		throw;
	}
	_pclose(pipe);
	std::clog.rdbuf(old);
	return result;
}

void externalCompressToOCS(std::string filePath, std::string ocsPath) {
	std::streambuf * old = std::clog.rdbuf(nullptr);
	std::string cmd = "compress.exe ";
	cmd.append(filePath);
	cmd.append(" ");
	cmd.append(ocsPath);
	
	FILE* pipe = _popen(cmd.c_str(), "r");
	_pclose(pipe);
	std::clog.rdbuf(old);
}

nlohmann::json readSaveData(std::string filePath) {
	std::ifstream f(filePath);
	f.seekg(0, std::ios::end);
	size_t size = f.tellg();
	std::string buffer(size, ' ');
	f.seekg(0);
	f.read(&buffer[0], size);

	std::string jsonStr = externalDecompress(filePath);

	return nlohmann::json::parse(jsonStr);
}

nlohmann::json steamSaveData;
nlohmann::json uwpSaveData;
nlohmann::json syncedSaveData;

int main(int argc, char ** argv)
{
	/// Setup CLI
	CLI::App app{ "Tool to synchronize and work with different Versions of Craftopia SaveData (Overcraft Save (OCS))" };
	app.allow_windows_style_options();

	CLI::Option * syncOpt = app.add_flag("--sync,-s", "Synchronize SaveData between versions");

	bool verbose = false;
	CLI::Option * verboseOpt = app.add_flag("--verbose,-v", verbose, "Enable verbose output");

	std::string exportPath = ".";
	CLI::Option * exportOpt = app.add_option("--export,-e", exportPath, "Export SaveData to JSON")->type_name("PATH")->default_str(".")->check(CLI::ExistingDirectory);
	
	std::string importPath = "SaveData.json";
	CLI::Option * importOpt = app.add_option("--import,-i", importPath, "Import SaveData from JSON")->type_name("PATH")->check(CLI::ExistingFile);

	CLI::Option * listWorldsOpt = app.add_flag("--list-worlds", "List loaded Worlds");
	CLI::Option * listCharsOpt = app.add_flag("--list-chars", "List loaded Character");

	app.add_flag("--test", "Do not overwrite SaveData and create SaveData_Test.ocs instead");// ->needs(syncOpt);

	app.add_flag("--no-backup", "Do not backup SaveData");

	app.add_flag("--steam", "Only overwrite Steam SaveData");
	app.add_flag("--uwp", "Only overwrite UWP SaveData");

	app.add_flag("--no-read", "Do not load SaveData at all");/*->needs(importOpt)
		->excludes(syncOpt)
		->excludes(exportOpt)
		->excludes(listWorldsOpt)
		->excludes(listCharsOpt);*/

	CLI11_PARSE(app, argc, argv);
	
	if (app.count_all() < 2) exit(0);

	/// Get Path to SaveData
	getSteamPath();
	if (steamSaveExists()) {
		if (verbose) std::cout << "Loading Steam SaveData from: " << steamSaveDataPath << std::endl;
		if(!app.count("--no-read")) steamSaveData = readSaveData(steamSaveDataPath);
	}
	getUWPPath();
	if (uwpSaveExists()) {
		if (verbose) std::cout << "Loading UWP SaveData from: " << uwpSaveDataPath << std::endl;
		if (!app.count("--no-read")) uwpSaveData = readSaveData(uwpSaveDataPath);
	}

	if (app.count("--list-worlds") > 0) {
		if (steamSaveExists()) {
			std::cout << "Steam:" << std::endl;
			for (const auto& world : steamSaveData["WorldListSave"]["value"].items()) {
				std::cout << "\t" << world.value()["name"].get<std::string>() << std::endl;
			}
		}
		if (uwpSaveExists()) {
			std::cout << "UWP:" << std::endl;
			for (const auto& world : steamSaveData["WorldListSave"]["value"].items()) {
				std::cout << "\t" << world.value()["name"].get<std::string>() << std::endl;
			}
		}
	}

	if (app.count("--list-chars") > 0) {
		if (steamSaveExists()) {
			std::cout << "Steam:" << std::endl;
			for (const auto& c : steamSaveData["CharacterListSave"]["value"].items()) {
				std::cout << "\t" << c.value()["charaMakeData"]["name"].get<std::string>() << std::endl;
			}
		}

		if (uwpSaveExists()) {
			std::cout << "UWP:" << std::endl;
			for (const auto& c : uwpSaveData["CharacterListSave"]["value"].items()) {
				std::cout << "\t" << c.value()["charaMakeData"]["name"].get<std::string>() << std::endl;
			}
		}
	}

	if (app.count("--sync") > 0) {
		if (steamSaveExists() && uwpSaveExists()) {
			struct stat result;
			time_t steamModTime;
			time_t uwpModTime;
			if (stat(steamSaveDataPath.c_str(), &result) == 0)
			{
				steamModTime = result.st_mtime;
			}
			if (stat(uwpSaveDataPath.c_str(), &result) == 0)
			{
				uwpModTime = result.st_mtime;
			}

			if (steamModTime > uwpModTime) {
				if (verbose) std::cout << "Steam has the latest SaveData" << std::endl;
				syncedSaveData = steamSaveData;

				/// Sync Worlds
				std::unordered_set<std::string> steamWorlds;
				for (const auto& world : steamSaveData["WorldListSave"]["value"].items()) {
					steamWorlds.insert(world.value()["name"].get<std::string>());
				}
				for (const auto& world : uwpSaveData["WorldListSave"]["value"].items()) {
					if (!steamWorlds.count(world.value()["name"].get<std::string>())) {
						if (verbose) std::cout << "Copying World from UWP: " << world.value()["name"].get<std::string>() << std::endl;
						syncedSaveData["WorldListSave"]["value"] += world.value();
					}
				}

				/// Sync Character
				std::unordered_set<std::string> steamCharacter;
				for (const auto& c : steamSaveData["CharacterListSave"]["value"].items()) {
					steamCharacter.insert(c.value()["charaMakeData"]["name"].get<std::string>());
				}
				for (const auto& c : uwpSaveData["CharacterListSave"]["value"].items()) {
					if (!steamCharacter.count(c.value()["charaMakeData"]["name"].get<std::string>())) {
						if (verbose) std::cout << "Copying Character from UWP: " << c.value()["charaMakeData"]["name"].get<std::string>() << std::endl;
						syncedSaveData["CharacterListSave"]["value"] += c.value();
					}
				}

				std::ofstream temp("temp");
				temp << syncedSaveData.dump();
				temp.close();


				if (app.count("--test") > 0) {
					externalCompressToOCS("temp", "SaveData_Test.ocs");
				} else {
					if(!app.count("--no-backup")) createBackup();
					if(!app.count("--steam")) externalCompressToOCS("temp", uwpSaveDataPath);
					if(!app.count("--uwp")) externalCompressToOCS("temp", steamSaveDataPath);
				}

				DeleteFileA("temp");
			}
			else {
				std::cout << "UWP has the latest SaveData" << std::endl;
				syncedSaveData = uwpSaveData;

				/// Sync Worlds
				std::unordered_set<std::string> uwpWorlds;
				for (const auto& world : uwpSaveData["WorldListSave"]["value"].items()) {
					uwpWorlds.insert(world.value()["name"].get<std::string>());
				}
				for (const auto& world : steamSaveData["WorldListSave"]["value"].items()) {
					if (!uwpWorlds.count(world.value()["name"].get<std::string>())) {
						if (verbose) std::cout << "Copying World from Steam: " << world.value()["name"].get<std::string>() << std::endl;
						syncedSaveData["WorldListSave"]["value"] += world.value();
					}
				}

				/// Sync Character
				std::unordered_set<std::string> uwpCharacter;
				for (const auto& c : uwpSaveData["CharacterListSave"]["value"].items()) {
					uwpCharacter.insert(c.value()["charaMakeData"]["name"].get<std::string>());
				}
				for (const auto& c : steamSaveData["CharacterListSave"]["value"].items()) {
					if (!uwpCharacter.count(c.value()["charaMakeData"]["name"].get<std::string>())) {
						if (verbose) std::cout << "Copying Character from Steam: " << c.value()["charaMakeData"]["name"].get<std::string>() << std::endl;
						syncedSaveData["CharacterListSave"]["value"] += c.value();
					}
				}

				std::ofstream temp("temp");
				temp << syncedSaveData.dump();
				temp.close();


				if (app.count("--test") > 0) {
					externalCompressToOCS("temp", "SaveData_Test.ocs");
				}
				else {
					if (!app.count("--no-backup")) createBackup();
					if (!app.count("--uwp")) externalCompressToOCS("temp", steamSaveDataPath);
					if (!app.count("--steam")) externalCompressToOCS("temp", uwpSaveDataPath);
				}

				DeleteFileA("temp");
			}
		}
	}

	if (app.count("--export") > 0) {
		if (verbose) std::cout << "Exporting SaveData to " << exportPath << std::endl;
		if (app.count("--sync") > 0) {
			std::string p = exportPath;
			p.append("\\SaveData.json");
			std::ofstream exp(p);
			exp << syncedSaveData.dump(4);
			exp.close();
		}
		else {
			if (steamSaveExists() && !(app.count("--uwp"))) {
				std::string p = exportPath;
				p.append("\\SaveData_Steam.json");
				std::ofstream steamExport(p);
				steamExport << steamSaveData.dump(4);
				steamExport.close();
			}

			if (uwpSaveExists() && !(app.count("--steam"))) {
				std::string p = exportPath;
				p.append("\\SaveData_UWP.json");
				std::ofstream uwpExport(p);
				uwpExport << uwpSaveData.dump(4);
				uwpExport.close();
			}
		}



		if (verbose) std::cout << "SaveData exported!" << std::endl;
	}

	if (app.count("--import") > 0) {
		if (!app.count("--no-backup")) createBackup();
		if (!app.count("--steam")) externalCompressToOCS(importPath, uwpSaveDataPath);
		if (!app.count("--uwp")) externalCompressToOCS(importPath, steamSaveDataPath);
	}
}

