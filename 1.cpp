
#include <stdio.h>
#include <Windows.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#pragma comment(lib, "SDL2.lib")

#pragma pack (push, 1)

typedef struct _SHP_TS_FRAME {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t  type;
    uint8_t  unknown[11];
    uint32_t offset;

} SHP_TS_FRAME;  // ShpTSFrame

typedef struct _SHP_TS_HEADER {
    uint16_t  Reserve;
    uint16_t  Width;
    uint16_t  Height;
    uint16_t  NumImages;

} SHP_TS_HEADER;

#pragma pack (pop)

ULONG RLE_Zeros_Decode(UCHAR *src, ULONG srclen, char *dest, int destIndex)
{
    ULONG RetLen = 0;
    ULONG i = 0;
    while (i < srclen)
    {
        UCHAR cmd = src[i++];
        if (cmd == 0)
        {
            UCHAR count = src[i++];
            RetLen += count;
            while (count-- > 0)
                dest[destIndex++] = 0;
        }
        else
        {
            dest[destIndex++] = cmd;
            ++RetLen;
        }
    }

    return RetLen;
}

int LoadPalettes(WCHAR *pwchFileName, SDL_Color *pColors)
{
    HANDLE hFile = CreateFileW(pwchFileName,
        GENERIC_READ,          
        FILE_SHARE_READ,
        NULL,               
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, 
        NULL);

    DWORD dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize != 768)  // 256 * 3
        printf("Palettes FileSize : %d !!!\n", dwFileSize);

    char *p = (char *)malloc(dwFileSize);
    ULONG RetLen;
    ReadFile(hFile, p, dwFileSize, &RetLen, NULL);
    if (dwFileSize != RetLen)
    {
        printf("ReadSize : %d !!!\n", RetLen);
        printf("dwFileSize != RetLen\n");
    }
    CloseHandle(hFile);

    for(ULONG i = 0; i < 256; i++)
    {
        pColors[i].r = p[i*3 + 2] << 2;
        pColors[i].g = p[i*3 + 1] << 2;
        pColors[i].b = p[i*3] << 2;
        pColors[i].a = 0xFF;

        //printf("%08X\n", *(PULONG)&pColors[i]);
    }

    free(p);
    return 0;
}

SHP_TS_HEADER *OpenShpTS(WCHAR *pwchFileName)
{
    SHP_TS_HEADER *pShpTsHeader = NULL;

    HANDLE hFile = CreateFileW(pwchFileName,
        GENERIC_READ,          
        FILE_SHARE_READ,
        NULL,               
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, 
        NULL);

    DWORD dwFileSize = GetFileSize(hFile, NULL);
    printf("FileSize : %d\n", dwFileSize);

    char *FileBuf = (char *)malloc(dwFileSize);

    ULONG RetLen;
    ReadFile(hFile, FileBuf, dwFileSize, &RetLen, NULL);
    if (dwFileSize != RetLen)
    {
        printf("ReadSize : %d !!!\n", RetLen);
        printf("dwFileSize != RetLen\n");
    }
    CloseHandle(hFile);

    pShpTsHeader = (SHP_TS_HEADER *)FileBuf;
    if (sizeof(SHP_TS_HEADER) + sizeof(SHP_TS_FRAME) * pShpTsHeader->NumImages > RetLen)
    {
        printf("Shp FileSize Error !\n");
        free(FileBuf);
        return 0;
    }

    if (!pShpTsHeader || pShpTsHeader->Reserve)  // Is Not ShpTS
    {
        printf("Is Not ShpTS !\n");
        free(FileBuf);
        return 0;
    }

    return pShpTsHeader;
}

int GetFrame(char *ShpBuf, char *FrameBuf, ULONG Index)
{
    SHP_TS_FRAME *pShpTsFrame = (SHP_TS_FRAME *)(ShpBuf + sizeof(SHP_TS_HEADER));

    // Pad the dimensions to an even number to avoid issues with half-integer offsets
    ULONG dataWidth  = pShpTsFrame[Index].w;
    ULONG dataHeight = pShpTsFrame[Index].h;

    char *pData = ShpBuf + pShpTsFrame[Index].offset;

    printf("[%2d]   %5x    %3d, %3d, %3d, %3d\n",
        Index+1, pShpTsFrame[Index].offset,
        pShpTsFrame[Index].x, pShpTsFrame[Index].y, pShpTsFrame[Index].w, pShpTsFrame[Index].h);

    if (pShpTsFrame[Index].type == 3)  // Format 3 provides RLE-zero compressed scanlines
    {
        for (ULONG j = 0; j < pShpTsFrame[Index].h; j++)
        {
            uint16_t length = *(uint16_t*)pData - 2;

            ULONG w = RLE_Zeros_Decode((UCHAR *)(pData + 2), length, FrameBuf, dataWidth * j);

            if (w < pShpTsFrame[Index].w)
            {
                printf("Line[%d] %d\n", j+1, w);
            }

            pData += length + 2;
        }
    }
    else
    {
        // Format 2 provides uncompressed length-prefixed scanlines
        // Formats 1 and 0 provide an uncompressed full-width row
        //                 ULONG length = Format == 2 ? s.ReadUInt16() - 2 : w;
        //                 for (var j = 0; j < h; j++)
        //                     s.ReadBytes(Data, dataWidth * j, length);
    }

    return 0;
}
int main()
{
    SHP_TS_HEADER *pShpTsHeader = OpenShpTS(L"d:\\ntclonmk.shp");
    ULONG NumImages = pShpTsHeader->NumImages;
    printf("ShpWidth : %d, ShpHeight : %d\n", pShpTsHeader->Width, pShpTsHeader->Height);
    ULONG ShpPicSize = pShpTsHeader->Width * pShpTsHeader->Height;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("SDL",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pShpTsHeader->Width, pShpTsHeader->Height,
        SDL_WINDOW_SHOWN /*| SDL_WINDOW_RESIZABLE*/);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);

    SDL_Surface *output_surface = SDL_CreateRGBSurface(0, pShpTsHeader->Width, pShpTsHeader->Height, 8, 0, 0, 0, 0);

    SDL_Color colors[256];
    LoadPalettes(L"d:\\RA2unittem.pal", colors);

    SDL_Texture *output_texture = SDL_CreateTextureFromSurface(renderer, output_surface);

    char *FrameBuf   = (char *)malloc(ShpPicSize);
    ULONG *TextureBuf = (ULONG *)malloc(ShpPicSize*4);
    printf("Frame  offset    X    Y    W    H\n");

    SHP_TS_FRAME *pShpTsFrame = (SHP_TS_FRAME *)((char*)pShpTsHeader + sizeof(SHP_TS_HEADER));

    SDL_Rect r;
    r.x = 0;
    r.y = 0;
    r.w = pShpTsHeader->Width;
    r.h = pShpTsHeader->Height;

    ULONG i = 0;

    int w, h, access;
    unsigned int format;

    SDL_QueryTexture( output_texture, &format, &access, &w, &h );  // format : SDL_PIXELFORMAT_ARGB8888

    bool running = true;

    while(running)
    {
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)  // SDL_KEYDOWN
            {  
                running = false;
                break;
            }
        }

        memset(TextureBuf, 0, r.w * r.h * 4);
        SDL_UpdateTexture(output_texture, &r, TextureBuf, r.w * 4);

        if (i == NumImages/2)
        {
            i = 0;
            SDL_Delay(600);
        }

        r.x = pShpTsFrame[i].x;
        r.y = pShpTsFrame[i].y;
        r.w = pShpTsFrame[i].w;
        r.h = pShpTsFrame[i].h;
        GetFrame((char *)pShpTsHeader, FrameBuf, i);

        for (ULONG j = 0; j < (ULONG)r.w * (ULONG)r.h; ++j)
        {
            UCHAR ColorIndex = FrameBuf[j];
            
            // printf("%d\n", ColorIndex);

            if (ColorIndex == 0)
                TextureBuf[j] = 0;
            else
                TextureBuf[j]   = *(PULONG)&colors[ColorIndex];

//             TextureBuf[j]   = 0xFF;  // B
//             TextureBuf[j+1] = 0x22;  // G
//             TextureBuf[j+2] = 0x33;  // R
//             TextureBuf[j+3] = 0xFF;  // A
        }

        SDL_UpdateTexture(output_texture, &r, TextureBuf, r.w * 4);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, output_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        ++i;
        SDL_Delay(90);
    }

    SDL_DestroyTexture(output_texture);
    SDL_FreeSurface(output_surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    free(TextureBuf);
    free(FrameBuf);
    free(pShpTsHeader);
    getchar();
    return 0;
}
