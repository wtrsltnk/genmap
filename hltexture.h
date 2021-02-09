#ifndef _HLTEXTURE_H_
#define _HLTEXTURE_H_

#include <glm/glm.hpp>
#include <string>

namespace valve
{

    class Texture
    {
    public:
        Texture();

        Texture(
            const std::string &name);

        virtual ~Texture();

        void ClearData();

        Texture *Copy() const;

        void CopyFrom(
            const Texture &from);

        void SetDimentions(
            int _width,
            int _height,
            int _bpp = 3);

        void SetData(
            int w,
            int h,
            int bpp,
            unsigned char *data,
            bool repeat = true);

        void DefaultTexture();

        glm::vec4 PixelAt(
            int x,
            int y) const;

        void SetPixelAt(
            const glm::vec4 &pixel,
            int x,
            int y);

        void Fill(
            const glm::vec4 &color);

        void Fill(
            const Texture &from);

        // expandBorder is used to puts a 1-pixel border around the copied texture so no neightbour leaking is occuring on an atlas
        void FillAtPosition(
            const Texture &from,
            const glm::vec2 &pos,
            bool expandBorder = false);

        void CorrectGamma(
            float gamma);

        void SetName(
            const std::string &name);

        void SetRepeat(
            bool repeat);

        const std::string &Name() const;

        int Width() const;

        int Height() const;

        int Bpp() const;

        bool Repeat() const;

        int DataSize() const;

        unsigned char *Data();

    private:
        std::string _name;
        int _width = 0;
        int _height = 0;
        int _bpp = 0;
        bool _repeat = true;
        unsigned char *_data = nullptr;
    };

} // namespace valve

#endif // _HLTEXTURE_H_
