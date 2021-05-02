/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/
// updates:
// 1-4-99	fixed file texture load and file read bug
// 2-8-99	fixed demand loaded sequence bug (thanks to Frans 'Otis' Bouma)

////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>

#include "common/mathlib.h"
#include "engine/studio.h"
#include "public/steam/steamtypes.h" // defines int32, required by studio.h
#include "studiomodel.h"
#include <stdio.h>

#pragma warning(disable : 4244) // double to float

////////////////////////////////////////////////////////////////////////

void StudioModel::UploadTexture(mstudiotexture_t *ptexture, byte *data, byte *pal)
{
    // unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight;
    int outwidth, outheight;
    int i, j;
    int row1[256], row2[256], col1[256], col2[256];
    byte *pix1, *pix2, *pix3, *pix4;
    byte *tex, *out;

    // convert texture to power of 2
    for (outwidth = 1; outwidth < ptexture->width; outwidth <<= 1)
        ;

    if (outwidth > 256)
        outwidth = 256;

    for (outheight = 1; outheight < ptexture->height; outheight <<= 1)
        ;

    if (outheight > 256)
        outheight = 256;

    tex = out = (byte *)malloc(outwidth * outheight * 4);

    for (i = 0; i < outwidth; i++)
    {
        col1[i] = (i + 0.25) * (ptexture->width / (float)outwidth);
        col2[i] = (i + 0.75) * (ptexture->width / (float)outwidth);
    }

    for (i = 0; i < outheight; i++)
    {
        row1[i] = (int)((i + 0.25) * (ptexture->height / (float)outheight)) * ptexture->width;
        row2[i] = (int)((i + 0.75) * (ptexture->height / (float)outheight)) * ptexture->width;
    }

    // scale down and convert to 32bit RGB
    for (i = 0; i < outheight; i++)
    {
        for (j = 0; j < outwidth; j++, out += 4)
        {
            pix1 = &pal[data[row1[i] + col1[j]] * 3];
            pix2 = &pal[data[row1[i] + col2[j]] * 3];
            pix3 = &pal[data[row2[i] + col1[j]] * 3];
            pix4 = &pal[data[row2[i] + col2[j]] * 3];

            out[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
            out[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
            out[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
            out[3] = 0xFF;
        }
    }

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 3 /*??*/, outwidth, outheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
    //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ptexture->width = outwidth;
    // ptexture->height = outheight;
    ptexture->index = texture;

    free(tex);
}

studiohdr_t *StudioModel::LoadModel(const char *modelname)
{
    FILE *fp = nullptr;
    long size;
    void *buffer;

    // load the model
    if (fopen_s(&fp, modelname, "rb") != 0)
    {
        printf("unable to open %s\n", modelname);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buffer = malloc(size);
    fread(buffer, size, 1, fp);

    byte *pin;
    studiohdr_t *phdr;
    mstudiotexture_t *ptexture;

    pin = (byte *)buffer;
    phdr = (studiohdr_t *)pin;

    ptexture = (mstudiotexture_t *)(pin + phdr->textureindex);
    if (phdr->textureindex != 0)
    {
        for (int i = 0; i < phdr->numtextures; i++)
        {
            UploadTexture(&ptexture[i], pin + ptexture[i].index, pin + ptexture[i].width * ptexture[i].height + ptexture[i].index);
        }
    }

    // UNDONE: free texture memory

    fclose(fp);

    return (studiohdr_t *)buffer;
}

studioseqhdr_t *StudioModel::LoadDemandSequences(const char *modelname)
{
    FILE *fp = nullptr;
    long size;
    void *buffer;

    // load the model
    if (fopen_s(&fp, modelname, "rb") != 0)
    {
        printf("unable to open %s\n", modelname);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buffer = malloc(size);
    fread(buffer, size, 1, fp);

    fclose(fp);

    return (studioseqhdr_t *)buffer;
}

void StudioModel::Init(const char *modelname)
{
    m_ptexturehdr = m_pstudiohdr = LoadModel(modelname);

    // preload textures
    if (m_pstudiohdr->numtextures == 0)
    {
        char texturename[256];

        strcpy_s(texturename, 256, modelname);
        texturename[strlen(texturename) - 4] = '\0';
        strcat_s(texturename, 256, "T.mdl");

        m_ptexturehdr = LoadModel(texturename);
    }

    // preload animations
    if (m_pstudiohdr->numseqgroups > 1)
    {
        for (int i = 1; i < m_pstudiohdr->numseqgroups; i++)
        {
            char seqgroupname[256];

            strcpy_s(seqgroupname, 256, modelname);
            seqgroupname[strlen(seqgroupname) - 4] = '\0';
            sprintf_s(seqgroupname, 256, "%s%02d.mdl", seqgroupname, i);

            m_panimhdr[i] = LoadDemandSequences(seqgroupname);
        }
    }
}
