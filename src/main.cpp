#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

using Byte = unsigned char;
#include "gl_image_processing.h"
template<class T> void release(T* p) { if (p) p->Release(); }
void check(HRESULT h) { if (FAILED(h)) throw std::runtime_error("Windows Imaging Component failed"); }

struct Image { int w, h; std::vector<Byte> pixels; };

Image load(IWICImagingFactory* factory, const wchar_t* filename) {
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    try {
        check(factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder));
        check(decoder->GetFrame(0, &frame));
        UINT width, height;
        check(frame->GetSize(&width, &height));
        check(factory->CreateFormatConverter(&converter));
        check(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom));
        Image result{static_cast<int>(width), static_cast<int>(height), std::vector<Byte>(size_t(width) * height * 4)};
        check(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(result.pixels.size()), result.pixels.data()));
        release(converter); release(frame); release(decoder);
        return result;
    } catch (...) { release(converter); release(frame); release(decoder); throw; }
}

void save(IWICImagingFactory* factory, const wchar_t* filename, const std::vector<Byte>& pixels, int w, int h) {
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* options = nullptr;
    try {
        check(factory->CreateStream(&stream));
        check(stream->InitializeFromFilename(filename, GENERIC_WRITE));
        check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
        check(encoder->Initialize(stream, WICBitmapEncoderNoCache));
        check(encoder->CreateNewFrame(&frame, &options));
        check(frame->Initialize(options));
        check(frame->SetSize(w, h));
        WICPixelFormatGUID format = GUID_WICPixelFormat8bppGray;
        check(frame->SetPixelFormat(&format));
        if (format != GUID_WICPixelFormat8bppGray) throw std::runtime_error("8-bit gray PNG unsupported");
        check(frame->WritePixels(h, w, static_cast<UINT>(pixels.size()), const_cast<Byte*>(pixels.data())));
        check(frame->Commit()); check(encoder->Commit());
        release(options); release(frame); release(encoder); release(stream);
    } catch (...) { release(options); release(frame); release(encoder); release(stream); throw; }
}

void csv(const char* filename, const std::vector<Byte>& values) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write text output");
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << unsigned(values[i]);
    }
}

int main() {
    const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(init)) { std::cerr<<"COM initialization failed\n"; return 1; }
    IWICImagingFactory* factory=nullptr;
    try {
        check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        const Image input=load(factory,L"Lenna.png");
        GlImageProcessing gl(input.w * 2, input.h * 2);
        const auto gray=gl.render(input.pixels,input.w,input.h,false);
        save(factory,L"Grayscale.png",gray,input.w,input.h);
        std::vector<Byte> grayLevels(gray.size());
        for(size_t i=0;i<gray.size();++i) grayLevels[i]=static_cast<Byte>(std::clamp(int(std::lround(gray[i]/17.0)),0,15));
        csv("Grayscale.txt",grayLevels);
        const auto dots=gl.render(gray,input.w,input.h,true);
        save(factory,L"Haftone.png",dots,input.w*2,input.h*2);
        std::vector<Byte> dotBits(dots.size());
        for(size_t i=0;i<dots.size();++i) dotBits[i]=dots[i]/255;
        csv("Halftone.txt",dotBits);
        const auto levels=gl.compute(gray,input.w,input.h,false);
        std::vector<Byte> dither(levels.size());
        for(size_t i=0;i<levels.size();++i) dither[i]=levels[i]*17;
        save(factory,L"FloyedSteinberg.png",dither,input.w,input.h);
        csv("FloyedSteinberg.txt",levels);
        const auto edgeBits=gl.compute(gray,input.w,input.h,true);
        csv("Canny.txt",edgeBits);
        std::vector<Byte> edges(edgeBits.size());
        for(size_t i=0;i<edges.size();++i) edges[i]=edgeBits[i]*255;
        save(factory,L"Canny.png",edges,input.w,input.h);
        std::cout<<"Generated four PNG and four TXT files from "<<input.w<<"x"<<input.h<<" Lenna.png\n";
        release(factory); CoUninitialize(); return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n"; release(factory); CoUninitialize(); return 1;
    }
}
