#include "hl1filesystem.h"

#include <spdlog/spdlog.h>

FileSystemSearchPath::FileSystemSearchPath(
    const std::filesystem::path &root)
    : _root(root)
{}

FileSystemSearchPath::FileSystemSearchPath(
    const std::string &root)
    : _root(std::filesystem::path(root))
{}

bool FileSystemSearchPath::IsInSearchPath(
    const std::string &filename)
{
    if (filename.find("pak0.pak") != _root.make_preferred().string().find("pak0.pak"))
    {
        return false;
    }

    return std::filesystem::path(filename).make_preferred().string()._Starts_with(_root.make_preferred().string());
}

std::string FileSystemSearchPath::LocateFile(
    const std::string &relativeFilename)
{
    if (std::filesystem::exists(_root / std::filesystem::path(relativeFilename)))
    {
        return _root.generic_string();
    }

    return "";
}

bool FileSystemSearchPath::LoadFile(
    const std::string &filename,
    valve::Array<valve::byte> &data)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);

    if (!file.is_open())
    {
        spdlog::error("File not found: {0}", filename);

        return false;
    }

    auto count = file.tellg();

    data.Allocate(count);
    file.seekg(0, std::ios::beg);
    file.read((char *)data.data, data.count);

    file.close();

    return true;
}

PakSearchPath::PakSearchPath(
    const std::filesystem::path &root)
    : FileSystemSearchPath(root)
{
    OpenPakFile();
}

PakSearchPath::PakSearchPath(
    const std::string &root)
    : FileSystemSearchPath(std::filesystem::path(root))
{
    OpenPakFile();
}

PakSearchPath::~PakSearchPath()
{
    if (_pakFile.is_open())
    {
        _pakFile.close();
    }
}

void PakSearchPath::OpenPakFile()
{
    if (!std::filesystem::exists(_root))
    {
        spdlog::error("{} does not exist", _root.string());

        return;
    }

    _pakFile.open(_root.string(), std::fstream::in | std::fstream::binary);

    if (!_pakFile.is_open())
    {
        spdlog::error("failed to open pak file {}", _root.string());

        return;
    }

    _pakFile.read((char *)&_header, sizeof(valve::hl1::tPAKHeader));

    if (_header.signature[0] != 'P' || _header.signature[1] != 'A' || _header.signature[2] != 'C' || _header.signature[3] != 'K')
    {
        _pakFile.close();

        spdlog::error("failed to open pak file {} due to wrong header {}", _root.string(), _header.signature);

        return;
    }

    _files.resize(_header.lumpsSize / sizeof(valve::hl1::tPAKLump));
    _pakFile.seekg(_header.lumpsOffset, std::fstream::beg);
    _pakFile.read((char *)_files.data(), _header.lumpsSize);

    spdlog::debug("loaded {} containing {} files", _root.string(), _files.size());
}

std::string PakSearchPath::LocateFile(
    const std::string &relativeFilename)
{
    for (auto f : _files)
    {
        if (relativeFilename == std::string(f.name))
        {
            return _root.string();
        }
    }

    return "";
}

bool PakSearchPath::LoadFile(
    const std::string &filename,
    valve::Array<valve::byte> &data)
{
    if (!_pakFile.is_open())
    {
        return false;
    }

    auto relativeFilename = filename.substr(_root.string().length() + 1);

    for (auto f : _files)
    {
        if (relativeFilename == std::string(f.name))
        {
            data.Allocate(f.filelen);
            _pakFile.seekg(f.filepos, std::fstream::beg);
            _pakFile.read((char *)data.data, f.filelen);

            return true;
        }
    }

    return false;
}

std::string FileSystem::LocateFile(
    const std::string &relativeFilename)
{
    for (auto &searchPath : _searchPaths)
    {
        auto result = searchPath->LocateFile(relativeFilename);
        if (!result.empty())
        {
            return result;
        }
    }

    return "";
}

bool FileSystem::LoadFile(
    const std::string &filename,
    valve::Array<valve::byte> &data)
{
    for (auto &searchPath : _searchPaths)
    {
        if (!searchPath->IsInSearchPath(filename))
        {
            continue;
        }

        if (searchPath->LoadFile(filename, data))
        {
            return true;
        }
    }

    return false;
}

void FileSystem::SetRootAndMod(
    const std::filesystem::path &root,
    const std::string &mod)
{
    _root = root;
    _mod = mod;

    auto modPath = _root / std::filesystem::path(_mod);

    if (std::filesystem::exists(modPath))
    {
        _searchPaths.push_back(std::make_unique<FileSystemSearchPath>(modPath));

        auto modPak = modPath / std::filesystem::path("pak0.pak");

        if (std::filesystem::exists(modPak))
        {
            _searchPaths.push_back(std::make_unique<PakSearchPath>(modPak));
        }
    }

    auto valvePath = _root / std::filesystem::path("valve");

    if (std::filesystem::exists(valvePath))
    {
        _searchPaths.push_back(std::make_unique<FileSystemSearchPath>(valvePath));

        auto valvePak = valvePath / std::filesystem::path("pak0.pak");

        if (std::filesystem::exists(valvePak))
        {
            _searchPaths.push_back(std::make_unique<PakSearchPath>(valvePak));
        }
    }
}

const std::filesystem::path &FileSystem::Root() const
{
    return _root;
}

const std::string &FileSystem::Mod() const
{
    return _mod;
}

void FileSystem::FindRootFromFilePath(
    const std::string &filePath)
{
    auto path = std::filesystem::path(filePath);

    if (!path.has_parent_path())
    {
        spdlog::error("given path ({}) has no parent path", filePath);

        return;
    }

    if (!std::filesystem::is_directory(path))
    {
        path = path.parent_path();
    }

    auto fn = path.filename();
    if (path.has_parent_path() && (fn == "maps" || fn == "models" || fn == "sprites" || fn == "sound" || fn == "gfx" || fn == "env"))
    {
        path = path.parent_path();
    }

    auto lastDirectory = path.filename().generic_string();

    do
    {
        for (auto &p : std::filesystem::directory_iterator(path))
        {
            if (p.is_directory())
            {
                continue;
            }

            if (p.path().filename() == "hl.exe" && p.path().has_parent_path())
            {
                SetRootAndMod(p.path().parent_path(), lastDirectory);

                return;
            }
        }

        lastDirectory = path.filename().generic_string();
        path = path.parent_path();

    } while (path.has_parent_path());
}
