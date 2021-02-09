#include "hl1wadasset.h"

#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>
#include <sstream>

using namespace valve::hl1;

WadAsset::WadAsset(
    IFileSystem *fs)
    : Asset(fs)
{}

WadAsset::~WadAsset()
{
    for (int i = 0; i < _header.lumpsCount; i++)
    {
        if (_loadedLumps[i] != nullptr)
        {
            delete[](_loadedLumps[i]);
            _loadedLumps[i] = nullptr;
        }
    }

    if (_file.is_open())
    {
        _file.close();
    }
}

bool WadAsset::Load(
    const std::string &filename)
{
    _header.lumpsCount = 0;
    _header.lumpsOffset = 0;
    _header.signature[0] = '\0';

    _file.open(filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!_file.is_open())
    {
        return false;
    }

    if (_file.tellg() == 0)
    {
        _file.close();
        return false;
    }

    _file.seekg(0, std::ios::beg);
    _file.read((char *)&_header, sizeof(tWADHeader));

    if (std::string(_header.signature, 4) != HL1_WAD_SIGNATURE)
    {
        _file.close();
        return false;
    }

    _lumps = new tWADLump[_header.lumpsCount];
    _file.seekg(_header.lumpsOffset, std::ios::beg);
    _file.read((char *)_lumps, _header.lumpsCount * sizeof(tWADLump));

    _loadedLumps = new byteptr[_header.lumpsCount];
    for (int i = 0; i < _header.lumpsCount; i++)
    {
        _loadedLumps[i] = nullptr;
    }

    return true;
}

bool WadAsset::IsLoaded() const
{
    return _file.is_open();
}

bool icasecmp(
    const std::string &l,
    const std::string &r)
{
    return l.size() == r.size() && std::equal(l.cbegin(), l.cend(), r.cbegin(),
                                              [](std::string::value_type l1, std::string::value_type r1) { return std::toupper(l1) == std::toupper(r1); });
}

int WadAsset::IndexOf(
    const std::string &name) const
{
    for (int l = 0; l < _header.lumpsCount; ++l)
        if (icasecmp(name, _lumps[l].name))
            return l;

    return -1;
}

valve::byteptr WadAsset::LumpData(
    int index)
{
    if (index >= _header.lumpsCount || index < 0)
        return nullptr;

    if (_loadedLumps[index] == nullptr)
    {
        _loadedLumps[index] = new byte[_lumps[index].size];
        _file.seekg(_lumps[index].offset, std::ios::beg);
        _file.read((char *)_loadedLumps[index], _lumps[index].size);
    }

    return _loadedLumps[index];
}

std::vector<std::string> split(
    const std::string &subject,
    const char delim = '\n')
{
    std::vector<std::string> result;

    std::istringstream f(subject);
    std::string s;
    while (getline(f, s, delim))
        result.push_back(s);

    return result;
}

// Answer from Stackoverflow: http://stackoverflow.com/a/9670795
template <class Stream, class Iterator>
void join(
    Stream &s,
    Iterator first,
    Iterator last,
    char const *delim = "\n")
{
    if (first >= last)
        return;

    s << *first++;

    for (; first != last; ++first)
        s << delim << *first;
}

std::vector<WadAsset *> WadAsset::LoadWads(
    const std::string &wads,
    IFileSystem *fs)
{
    std::vector<WadAsset *> result;

    std::istringstream f(wads);
    std::string s;
    while (getline(f, s, ';'))
    {
        auto wadPath = std::filesystem::path(s);
        auto location = fs->LocateFile(wadPath.filename().string());
        if (location.empty())
        {
            continue;
        }

        WadAsset *wad = new WadAsset(fs);

        auto fullWadPath = location / wadPath.filename();

        if (wad->Load(fullWadPath.string()))
        {
            result.push_back(wad);
        }
        else
        {
            spdlog::error("Unable to load wad files @ {0}", fullWadPath.string());
            delete wad;
        }
    }

    return result;
}

std::string WadAsset::FindWad(
    const std::string &wad,
    const std::vector<std::string> &hints)
{
    std::string tmp = wad;
    std::replace(tmp.begin(), tmp.end(), '/', '\\');
    std::vector<std::string> wadComponents = split(tmp, '\\');

    for (std::vector<std::string>::const_iterator i = hints.cbegin(); i != hints.cend(); ++i)
    {
        std::string tmp = ((*i) + "\\" + wadComponents[wadComponents.size() - 1]);
        std::ifstream f(tmp.c_str());
        if (f.good())
        {
            f.close();
            return tmp;
        }
    }

    // When the wad file is not found, we might wanna check original wad string for a possible mod directory
    std::string lastTry = hints[hints.size() - 1] + "\\" + wadComponents[wadComponents.size() - 2] + "\\" + wadComponents[wadComponents.size() - 1];
    std::ifstream f(lastTry.c_str());
    if (f.good())
    {
        f.close();
        return lastTry;
    }

    return wad;
}

void WadAsset::UnloadWads(
    std::vector<WadAsset *> &wads)
{
    while (wads.empty() == false)
    {
        WadAsset *wad = wads.back();
        wads.pop_back();
        delete wad;
    }
}
