#include <Windows.h>
#include <stdio.h>
#include <sstream>
#include <fstream>

#include<nlohmann/json.hpp>
using json = nlohmann::json;

#include <openssl/md5.h>
#include <openssl/evp.h>

constexpr int BUFFSIZE = 16384;

// https://stackoverflow.com/questions/865668/parsing-command-line-arguments-in-c
class InputParser {
public:
    InputParser(int& argc, char** argv) {
        for (int i = 1; i < argc; ++i)
            this->tokens.push_back(std::string(argv[i]));
    }

    const std::string& getCmdOption(const std::string& option) const {
        std::vector<std::string>::const_iterator itr;
        itr = std::find(this->tokens.begin(), this->tokens.end(), option);
        if (itr != this->tokens.end() && ++itr != this->tokens.end()) {
            return *itr;
        }
        static const std::string empty_string("");
        return empty_string;
    }

    bool cmdOptionExists(const std::string& option) const {
        return std::find(this->tokens.begin(), this->tokens.end(), option)
            != this->tokens.end();
    }
private:
    std::vector <std::string> tokens;
};

double GetFileSize(std::ifstream& file)
{
    std::streampos fsize = file.tellg();
    file.seekg(0, std::ios::end);
    fsize = file.tellg() - fsize;

    file.seekg(0, std::ios::beg);

    return (double)fsize;
}

std::string HashFileMD5(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        printf("Failed to open file: %s\n", filename.c_str());
        return std::string();
    }

    EVP_MD_CTX* md5Context = EVP_MD_CTX_new();
    EVP_MD_CTX_init(md5Context);
    EVP_DigestInit_ex(md5Context, EVP_md5(), nullptr);
    const size_t bufferSize = 4096;
    char buffer[bufferSize];

    double fsize = GetFileSize(file);

    std::vector<int> multiples;
    for (int i = 0; i <= 100; i += 5)
    {
        if (i == 0)
            continue;

        multiples.push_back(i);
    }

    printf("Calculating hash for %s: ", filename.c_str());

    std::string str;
    while (!file.eof()) {
        file.read(buffer, bufferSize);
        EVP_DigestUpdate(md5Context, buffer, file.gcount());

        double percent = (file.tellg() / fsize) * 100;

        for (unsigned int i = 0; i < multiples.size(); i++)
        {
            if (multiples[i] == (int)std::round(percent))
            {
                multiples.erase(multiples.begin() + i);

                if (!str.empty())
                    printf(std::string(str.length() - 1, '\b').c_str());

                str = std::to_string((int)std::round(percent));
                str += "%%";
                printf(str.c_str());
            }
        }
    }
    if (str.empty())
        printf("100%% \n");
    else
        printf("\n");

    std::array<uint8_t, 16> result;
    EVP_DigestFinal_ex(md5Context, result.data(), nullptr);
    file.close();
    EVP_MD_CTX_free(md5Context);

    std::stringstream ss;

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result[i]);
    }
    return ss.str();
}

bool CheckConfigHashes(std::string sConfigPath)
{
    std::ifstream ifs(sConfigPath);
    if (!ifs) {
        printf("Failed to open file: %s", sConfigPath.c_str());
        return false;
    }

    json data;

    try {
        data = json::parse(ifs);
        ifs.close();
    }
    catch (std::exception e) {
        ifs.close();

        printf("Failed to read JSON in %s\n", sConfigPath.c_str());
        printf("Exception: %s\n", e.what());

        return false;
    }

    std::vector<std::string> passed;
    std::vector<std::string> failed;
    int filecount = 0;

    for (auto& file : data["Files"])
    {
        filecount++;

        std::string providedHash = file["Hash"];
        for (auto& c : providedHash)
        {
            c = tolower(c);
        }

        std::string hash = HashFileMD5(file["Name"]);
        if (hash.empty())
            continue;

        if (providedHash == hash)
            passed.push_back(file["Name"]);
        else
            failed.push_back(file["Name"]);
    }

    printf("\n%zd/%d Files Passed\n", passed.size(), filecount);

    if (failed.size())
        printf("\nFailed:\n");
    for (auto& file : failed)
        printf("%s\n", file.c_str());

    return true;
}

bool CheckInputHashes(std::string sInputPath, bool bOutput, std::string sOutputPath)
{

    std::filesystem::path path(sInputPath);
    json data = json::parse("{\"Files\": []}");
    bool bIsDir = false;

    if (std::filesystem::is_directory(path))
    {
        bIsDir = true;
        for (auto& entry : std::filesystem::recursive_directory_iterator(path))
        {
            if (std::filesystem::is_directory(entry.path()))
                continue;

            json j = {
                {"Name", entry.path().string()},
                {"Hash", ""}
            };

            data["Files"].push_back(j);
        }
    }
    else
    {
        json j = {
            {"Name", path.string()},
            {"Hash", ""}
        };
        data["Files"].push_back(j);
    }

    struct entry
    {
        std::string name;
        std::string hash;
    };

    std::vector<entry> entries;

    for (auto& file : data["Files"])
    {
        std::string hash = HashFileMD5(file["Name"]);
        if (hash.empty())
        {
            entries.push_back({ file["Name"], "Failed to get MD5 hash"});
            continue;
        }

        entries.push_back({ file["Name"], hash });
    }

    printf("\nHashes:\n");
    for (auto& e : entries)
        printf("%s: %s\n", e.name.c_str(), e.hash.c_str());

    if (bOutput)
    {
        std::ofstream ofs(sOutputPath);
        if (!ofs) {
            printf("Failed to open file: %s", sOutputPath.c_str());
            return false;
        }

        json outData = json::parse("{\"Files\": []}");

        for (auto& e : entries)
        {
            if (e.hash == "Failed to get MD5 hash")
                continue;

            std::string name = e.name;
            if (std::filesystem::is_directory(path))
                name = std::filesystem::relative(std::filesystem::path(e.name), path).string();

            json j = {
                {"Name", name},
                {"Hash", e.hash}
            };

            outData["Files"].push_back(j);
        }

        std::string JSON = outData.dump(4);
        ofs.write(JSON.c_str(), JSON.size());
        ofs.close();
    }

    return true;
}

std::vector<std::string> Split(std::string str, std::string delimiter)
{
    size_t pos = 0;
    std::vector<std::string> tokens;
    while ((pos = str.find(delimiter)) != std::string::npos) {
        tokens.push_back(str.substr(0, pos));
        str.erase(0, pos + delimiter.length());
    }

    tokens.push_back(str);

    return tokens;
}

bool ConvertFile(std::string sInputPath, bool bOutput, std::string sOutputPath)
{
    std::ifstream ifs(sInputPath);
    if (!ifs) {
        printf("Failed to open file: %s", sInputPath.c_str());
        return false;
    }

    json data = json::parse("{\"Files\": []}");
    std::string line;

    while (std::getline(ifs, line))
    {
        if (line.starts_with(";"))
            continue;

        line.erase(remove(line.begin(), line.end(), '*'), line.end());

        std::vector<std::string> tokens = Split(line, " ");

        std::string name;
        for (int i = 1; i < tokens.size(); i++)
        {
            name += tokens[i];
            name += " ";
        }
        name.erase(name.find_last_of(" "));

        json j = {
            {"Name", name},
            {"Hash", tokens[0]}
        };
        data["Files"].push_back(j);
    }

    std::string path = "config.json";
    if (bOutput)
        path = sOutputPath;

    std::ofstream ofs(path);
    if (!ofs) {
        printf("Failed to open file: %s", sOutputPath.c_str());
        return false;
    }

    std::string JSON = data.dump(4);
    ofs.write(JSON.c_str(), JSON.size());
    ofs.close();
}

int main(int argc, char** argv)
{
    InputParser input(argc, argv);
    if (input.cmdOptionExists("-h") || input.cmdOptionExists("--help"))
    {
        printf("-h | --help - Prints this help message\n");
        printf("--config - Runs the MD5 checker using the provided config\n");
        printf("--input - Generates a config with the files provided\n");
        printf("--convert - Converts a .md5 config into a json config\n");
    }

    bool bConfig = false;
    std::string sConfigPath;
    if (input.cmdOptionExists("--config"))
    {
        bConfig = true;
        sConfigPath = input.getCmdOption("--config");
    }

    bool bInput = false;
    std::string sInputPath;
    if (input.cmdOptionExists("--input") && !bConfig)
    {
        bInput = true;
        sInputPath = input.getCmdOption("--input");
    }

    bool bConvert = false;
    if (input.cmdOptionExists("--convert") && !bConfig && !bInput)
    {
        bConvert = true;
        sInputPath = input.getCmdOption("--convert");
    }

    bool bOutput = false;
    std::string sOutputPath;
    if (input.cmdOptionExists("--out") && !bConfig && (bInput || bConvert))
    {
        bOutput = true;
        sOutputPath = input.getCmdOption("--out");
    }

    if (bConfig)
        return CheckConfigHashes(sConfigPath);

    if (bInput)
        return CheckInputHashes(sInputPath, bOutput, sOutputPath);
    
    if (bConvert)
        return ConvertFile(sInputPath, bOutput, sOutputPath);

	return EXIT_SUCCESS;
}