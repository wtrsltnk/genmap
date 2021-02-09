#ifndef _HL1WADASSET_H_
#define _HL1WADASSET_H_

#include "hl1bsptypes.h"
#include "hltypes.h"

#include <fstream>
#include <string>
#include <vector>

namespace valve
{

    namespace hl1
    {

        class WadAsset :
            public Asset
        {
        public:
            WadAsset(
                IFileSystem *fs);

            virtual ~WadAsset();

            bool Load(
                const std::string &filename);

            bool IsLoaded() const;

            int IndexOf(
                const std::string &name) const;

            byteptr LumpData(
                int index);

            static std::string FindWad(
                const std::string &wad,
                const std::vector<std::string> &hints);

            static std::vector<WadAsset *> LoadWads(
                const std::string &wads,
                IFileSystem *fs);

            static void UnloadWads(
                std::vector<WadAsset *> &wads);

        private:
            std::ifstream _file;
            tWADHeader _header;
            tWADLump *_lumps = nullptr;
            byteptr *_loadedLumps = nullptr;
        };

    } // namespace hl1

} // namespace valve

#endif // _HL1WADASSET_H_
