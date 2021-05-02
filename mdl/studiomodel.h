/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#ifndef STUDIOMODEL_H
#define STUDIOMODEL_H

#include "engine/studio.h"
#include "renderapi.hpp"
#include <memory>
#include <vector>

#include <glm/glm.hpp>

class StudioModel
{
public:
    void Init(const char *modelname);

private:
    // internal data
    studiohdr_t *m_pstudiohdr;
    mstudiomodel_t *m_pmodel;

    studiohdr_t *m_ptexturehdr;
    studioseqhdr_t *m_panimhdr[32];

    studiohdr_t *LoadModel(const char *modelname);
    studioseqhdr_t *LoadDemandSequences(const char *modelname);

    void UploadTexture(mstudiotexture_t *ptexture, byte *data, byte *pal);

    template <typename T>
    T *get(int index)
    {
        return (T *)((byte *)m_pstudiohdr + index);
    }

    template <typename T>
    T *get_texture(int index)
    {
        if (m_ptexturehdr != nullptr)
        {
            return (T *)((byte *)m_ptexturehdr + index);
        }

        return (T *)((byte *)m_pstudiohdr + index);
    }

    friend class StudioEntity;
};

class StudioEntity
{
public:
    StudioEntity(StudioModel *model);

    void DrawModel(RenderApi<glm::vec3, glm::vec4, glm::vec2> &renderer);
    void AdvanceFrame(float dt);

    void ExtractBbox(float *mins, float *maxs);

    int SetSequence(int iSequence);
    int GetSequence(void);
    void GetSequenceInfo(float *pflFrameRate, float *pflGroundSpeed);

    float SetController(int iController, float flValue);
    float SetMouth(float flValue);
    float SetBlending(int iBlender, float flValue);
    int SetBodygroup(int iGroup, int iValue);
    int SetSkin(int iValue);

private:
    int m_sequence = 0;                   // sequence index
    float m_frame = 0;                    // frame
    int m_bodynum = 0;                    // bodypart selection
    int m_skinnum = 0;                    // skin group selection
    byte m_controller[4] = {0, 0, 0, 0};  // bone controllers
    byte m_blending[2] = {0, 0};          // animation blending
    byte m_mouth = 0;                     // mouth position

    StudioModel *_model = nullptr;

    void CalcBoneAdj(void);
    void CalcBoneQuaternion(int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *q);
    void CalcBonePosition(int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *pos);
    void CalcRotations(vec3_t *pos, vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f);
    mstudioanim_t *GetAnim(mstudioseqdesc_t *pseqdesc);
    void SlerpBones(vec4_t q1[], vec3_t pos1[], vec4_t q2[], vec3_t pos2[], float s);
    void SetUpBones(void);

    void DrawPoints(RenderApi<glm::vec3, glm::vec4, glm::vec2> &renderer);

    void Lighting(float *lv, int bone, int flags, vec3_t normal);
    void Chrome(int *chrome, int bone, vec3_t normal);

    void SetupLighting(void);

    void SetupModel(int bodypart);
};

extern vec3_t g_vright; // needs to be set to viewer's right in order for chrome to work
extern float g_lambert; // modifier for pseudo-hemispherical lighting

#endif // STUDIOMODEL_H
