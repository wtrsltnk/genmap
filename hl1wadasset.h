#ifndef _HL1WADASSET_H_
#define _HL1WADASSET_H_

#include "hltypes.h"
#include "hl1bsptypes.h"

#include <string>
#include <vector>
#include <fstream>

namespace valve
{

    namespace hl1
    {

        class WadAsset : 
            public Asset
        {
            std::ifstream _file;
            tWADHeader _header;
            tWADLump* _lumps;
            byteptr* _loadedLumps;

        public:
            WadAsset(
                IFileSystem *fs);
            
            virtual ~WadAsset();
            
            bool Load(
                const std::string& filename);
            
            bool IsLoaded() const;
            
            int IndexOf(
                const std::string& name) const;
            
            const byteptr LumpData(
                int index);

            static std::string FindWad(
                const std::string& wad,
                const std::vector<std::string>& hints);
            
            static std::vector<WadAsset*> LoadWads(
                const std::string& wads,
                const std::string& bspLocation,
                IFileSystem *fs);
            
            static void UnloadWads(
                std::vector<WadAsset*>& wads);

        };

    } // hl1

} // valve

#endif // _HL1WADASSET_H_