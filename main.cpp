#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

class Image
{
    template<typename T>
    void readint(std::ifstream& f, T& val)
    {   //endian-independant read
        //static_assert(sizeof(T) <= 4); //requires c++17
        uint8_t buf[4];
        f.read((char*)buf, sizeof(val));
        val = 0;
        for (int i = sizeof(val) - 1; i >= 0; i--)
            val += (buf[i] << (8 * i));
    }

    template<typename T>
    void writeint(std::ofstream& f, T val)
    {   //endian-independant write
        //static_assert(sizeof(T) <= 4); //requires c++17
        uint8_t buf[4];
        for (int i = sizeof(val) - 1; i >= 0; i--)
            buf[i] = (val >> 8 * i) & 0xff;
        f.write((char*)buf, sizeof(val));
    }

    //based on BITMAPFILEHEADER, struct size will be off
    struct cfileheader
    {
        char type[2];
        uint32_t size;
        uint32_t reserved;
        uint32_t offset;
    };

    //based on BITMAPINFOHEADER, struct size will be off
    struct cinfoheader
    {
        uint32_t struct_size;
        uint32_t width;
        uint32_t height;
        uint16_t planes;
        uint16_t bitcount;
        uint32_t compression;
        uint32_t image_size;
        uint32_t xpermeter;
        uint32_t ypermeter;
        uint32_t colors_used;
        uint32_t colors_important;
    };

    Image(const Image&) {} //lets make these inaccessible
    Image& operator = (Image const&) {}
public:
    Image();
    int getActualWidthInBytes();
    bool readfile(const char* filename);
    bool writefile(const char* filename);
    bool isopen;
    int  width, height;
    int  width_in_bytes;
    int  bitcount;
    cfileheader fileheader;
    cinfoheader info;
    std::vector<uint32_t> palette;
    std::vector<uint8_t> image;
    uint32_t getpixel(int x, int y)
    {
        switch (bitcount)
        {
            case 1:
            {
                const int rowindex = (height - y - 1) * width_in_bytes;
                const uint8_t bit = (image[rowindex + x / 8] >> (7 - (x % 8))) & 1;
                return palette[bit];
            }
            case 4:
            {
                const int start = (height - y - 1) * width_in_bytes;
                uint8_t pal = image[start + x / 2];
                if (!(x % 2)) pal >>= 4;
                return palette[pal & 0xf];
            }
            case 8:
            {
                //find the index of pixel at x/y
                //the pixel should have a value between 0 to 256
                //get the color from palette
                const int i = (height - y - 1)* width_in_bytes + x;
                const uint8_t pal = image[i];
                return palette[pal];
            }
            case 24:
            {
                const int i = (height - y - 1) * width_in_bytes + x * 3;
                return image[i + 2] + (image[i + 1] << 8) + (image[i + 0] << 16);
            }
            case 32:
            {
                const int i = (height - y - 1) * width_in_bytes + x * 4;
                return image[i + 2] + (image[i + 1] << 8) + (image[i + 0] << 16);
            }
        }
        return 0;
    }
};

Image::Image():
        isopen{ false }, fileheader{}, info{},
        bitcount{}, width{}, height{}, width_in_bytes{}
{
}

bool Image::readfile(const char* filename)
{
    std::ifstream fin(filename, std::ios::binary);
    if (!fin) { std::cout << "open failed " << filename << '\n'; return false; }

    fin.read(fileheader.type, 2);
    if (strncmp(fileheader.type, "BM", 2) != 0) return false;
    readint(fin, fileheader.size);
    readint(fin, fileheader.reserved);
    readint(fin, fileheader.offset);

    readint(fin, info.struct_size);
    readint(fin, info.width);
    readint(fin, info.height);
    readint(fin, info.planes);
    readint(fin, info.bitcount);
    readint(fin, info.compression);
    readint(fin, info.image_size);
    readint(fin, info.xpermeter);
    readint(fin, info.ypermeter);
    readint(fin, info.colors_used);
    readint(fin, info.colors_important);

    width = info.width;
    height = info.height;
    bitcount = info.bitcount;

    if (info.struct_size != 40)
    {
        printf("wrong structure size %d\n", info.struct_size);
        return false;
    }

    std::vector<uint16_t> bitcheck {1,4,8,24,32};
    if(std::find(bitcheck.begin(), bitcheck.end(), bitcount) == bitcheck.end())
    {
        printf("cannot handle this bitcount %d\n", bitcount);
        return false;
    }

    int palette_size = (bitcount > 8) ? 0 : (1 << bitcount);
    palette.resize(palette_size);
    for (auto &e : palette)
    {
        //BGRA -> ABGR
        uint8_t buf[4] {};
        fin.read((char*)buf, 4);
        e = buf[2] | (buf[1] << 8) | (buf[0] << 16) | (buf[3] << 24);
    }

    if(fin.tellg() != fileheader.offset)
    { printf("error reading image\n"); return false; }

    width_in_bytes = ((width * info.bitcount + 31) / 32) * 4;
    image.resize(width_in_bytes * height);
    fin.read((char*)image.data(), image.size());
    isopen = true;
    return true;
}
int Image::getActualWidthInBytes(){
    this->width_in_bytes = ((width * info.bitcount + 31) / 32) * 4;
}
bool Image::writefile(const char* filename)
{
    if (!isopen) return false;
    std::ofstream fout(filename, std::ios::binary);
    if (!fout) { std::cout << "open failed " << filename << '\n'; return false; }
   this->width=width+15;

    height+=30;

    fout.write((char*)fileheader.type, 2);
    writeint(fout, fileheader.size);
    writeint(fout, fileheader.reserved);
    writeint(fout, fileheader.offset);
    writeint(fout, info.struct_size);
    writeint(fout, info.width+32);
    writeint(fout, info.height+30);
    writeint(fout, info.planes);
    writeint(fout, info.bitcount);
    writeint(fout, info.compression);
    writeint(fout, info.image_size);
    writeint(fout, info.xpermeter);
    writeint(fout, info.ypermeter);
    writeint(fout, info.colors_used);
    writeint(fout, info.colors_important);
    int temp=rand()%255;

    for(int i=0;i<=15;i++){
       image.insert(image.begin()+i*width_in_bytes,width_in_bytes,temp);
    }
    temp=rand()%255;
    for(int i=0;i<=15;i++){
        image.insert(image.end()-i*width_in_bytes-15,width_in_bytes,temp);
    }


//        }
auto innerImage=image;
    getActualWidthInBytes();
    std::cout<<std::endl<<width_in_bytes<<" "<<width<<" "<<bitcount;
    if(bitcount==8) {
        for (int i = 0; i <= image.size(); i += width_in_bytes) {

            image.insert(image.begin() + i, 16, 100);

        }
        for (int i = image.size(); i > width_in_bytes; i -= width_in_bytes) {

            image.insert(image.end() + width_in_bytes - i - 1, 16, 100);

        }
    }else if(bitcount==24){
        for (int i = 0; i <= image.size(); i += width_in_bytes) {

            image.insert(image.begin() + i, 16*3, 100);

        }
        for (int i = image.size(); i > width_in_bytes; i -= width_in_bytes) {

            image.insert(image.end() + width_in_bytes - i , 16*3, 100);


        }
    }
        //image[i*width_in_bytes+j]=250;
        //
        //



    int count=0;
    for (auto &e : palette)
    {
        int med=0;
        //ABGR -> BGRA
        uint8_t buf[4]{};

        buf[0] = (e >> 16) & 0xff;
        buf[1] = (e >>  8) & 0xff;
        buf[2] = (e >>  0) & 0xff;
        buf[3] = (e >> 24) & 0xff;
//        for(auto &j:buf){
//        med+=j;
//
//        std::cout<<(int16_t)j<<" ";}
//        med/=4;
//       // med+=20;
//
//        buf[0] = med;
//        buf[1] = med;
//        buf[2] = med;
//        buf[3] = med;
        std::cout<<std::endl;
        fout.write((char*)buf, 4);
        count++;
    }

    fout.write((char*)image.data(), image.size());

    return true;
}
int main(){
    Image bmpFile;
    bmpFile.readfile("CatSample.bmp");
    Image bmpFileTc;
    bmpFileTc.readfile("tc.bmp");
    std::cout<< bmpFile.isopen<<std::endl<<bmpFile.info.bitcount;
    std::cout<<bmpFile.writefile("result.bmp");
    std::cout<< bmpFileTc.isopen<<std::endl<<bmpFileTc.info.bitcount;
    std::cout<<bmpFileTc.writefile("resultTc.bmp");
}