#ifndef _HLTYPES_H_
#define _HLTYPES_H_

#include <glm/glm.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace valve
{

    typedef std::map<std::string, std::string> KeyValueList;

    typedef unsigned char byte;
    typedef byte *byteptr;

    template <typename T>
    class Array
    {
        bool _deleteOnDestruct;

    public:
        Array() : count(0), data(nullptr), _deleteOnDestruct(false) {}
        Array(int count) : _deleteOnDestruct(true) { this->Allocate(count); }
        Array(int count, T *data) : count(count), data(data), _deleteOnDestruct(false) {}
        virtual ~Array()
        {
            if (this->_deleteOnDestruct) this->Delete();
        }

        int count;
        T *data;

        operator T *(void)const { return data; }
        const T &operator[](int index) const { return this->data[index]; }
        T &operator[](int index) { return this->data[index]; }

        virtual void Allocate(int count)
        {
            this->count = count;
            this->data = this->count > 0 ? new T[this->count] : nullptr;
        }

        void Map(int count, T *data)
        {
            this->count = count;
            this->data = data;
        }

        virtual void Delete()
        {
            if (this->data != nullptr) delete[] this->data;
            this->data = nullptr;
            this->count = 0;
        }
    };

#define CHUNK (4096)

    template <class T>
    class List
    {
        int size;
        int count;
        T *data;

    public:
        List() : size(CHUNK), data(new T[CHUNK]), count(0) {}
        virtual ~List() { this->Clear(); }

        operator T *(void)const { return data; }
        const T &operator[](int i) const { return data[i]; }
        T &operator[](int i) { return data[i]; }

        int Count() const { return count; }

        void Add(T &src)
        {
            if (count >= size)
            {
                //resize
                T *n = new T[size + CHUNK];
                for (int i = 0; i < size; i++)
                    n[i] = data[i];
                delete[] data;
                data = n;
                size += CHUNK;
            }

            data[count] = src;
            count++;
        }

        void Clear()
        {
            if (this->data != nullptr)
                delete this->data;
            this->data = nullptr;
            this->size = this->count = 0;
        }
    };

    typedef struct sVertex
    {
        glm::vec3 position;
        glm::vec2 texcoords[2];
        glm::vec3 normal;
        int bone;

    } tVertex;

    typedef struct sFace
    {
        int firstVertex;
        int vertexCount;
        unsigned int lightmap;
        unsigned int texture;

        int flags;
        glm::vec4 plane;

    } tFace;

    class IFileSystem
    {
    public:
        virtual std::string LocateFile(const std::string &relativeFilename) = 0;
        virtual bool LoadFile(const std::string &filename, Array<byte> &data) = 0;
    };

    class Asset
    {
    protected:
        IFileSystem *_fs;

    public:
        Asset(IFileSystem *fs) : _fs(fs) {}
        virtual ~Asset() {}

        virtual bool Load(const std::string &filename) = 0;
    };

} // namespace valve

#endif // _HLTYPES_H_
