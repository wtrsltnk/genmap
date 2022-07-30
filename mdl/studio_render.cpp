/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/
// studio_render.cpp: routines for drawing Half-Life 3DStudio models
// updates:
// 1-4-99	fixed AdvanceFrame wraping bug

#include <glad/glad.h>

#pragma warning(disable : 4244) // double to float

////////////////////////////////////////////////////////////////////////

#include "common/mathlib.h"
#include "engine/studio.h"
#include "public/steam/steamtypes.h" // defines int32, required by studio.h
#include "studiomodel.h"

#include <glm/glm.hpp>

vec3_t g_vright; // needs to be set to viewer's right in order for chrome to work
float g_lambert = 1.5;

////////////////////////////////////////////////////////////////////////

vec3_t g_xformverts[MAXSTUDIOVERTS];  // transformed vertices
vec3_t g_lightvalues[MAXSTUDIOVERTS]; // light surface normals
vec3_t *g_pxformverts;
vec3_t *g_pvlightvalues;

vec3_t g_lightvec;                  // light vector in model reference frame
vec3_t g_blightvec[MAXSTUDIOBONES]; // light vectors in bone reference frames
int g_ambientlight;                 // ambient world light
float g_shadelight;                 // direct world light
vec3_t g_lightcolor;
vec4_t g_adj; // non persistant, make static

int g_smodels_total; // cookie

float g_bonetransform[MAXSTUDIOBONES][4][4]; // bone transformation matrix

int g_chrome[MAXSTUDIOVERTS][2];      // texture coords for surface normals
int g_chromeage[MAXSTUDIOBONES];      // last time chrome vectors were updated
vec3_t g_chromeup[MAXSTUDIOBONES];    // chrome vector "up" in bone reference frames
vec3_t g_chromeright[MAXSTUDIOBONES]; // chrome vector "right" in bone reference frames

////////////////////////////////////////////////////////////////////////

StudioEntity::StudioEntity(StudioModel *model)
    : _model(model)
{}

////////////////////////////////////////////////////////////////////////

int StudioEntity::GetSequence()
{
    return m_sequence;
}

int StudioEntity::SetSequence(int iSequence)
{
    if (iSequence > _model->m_pstudiohdr->numseq)
        iSequence = 0;
    if (iSequence < 0)
        iSequence = _model->m_pstudiohdr->numseq - 1;

    m_sequence = iSequence;
    m_frame = 0;

    return m_sequence;
}

void StudioEntity::ExtractBbox(float *mins, float *maxs)
{
    mstudioseqdesc_t *pseqdesc;

    pseqdesc = _model->get<mstudioseqdesc_t>(_model->m_pstudiohdr->seqindex);

    mins[0] = pseqdesc[m_sequence].bbmin[0];
    mins[1] = pseqdesc[m_sequence].bbmin[1];
    mins[2] = pseqdesc[m_sequence].bbmin[2];

    maxs[0] = pseqdesc[m_sequence].bbmax[0];
    maxs[1] = pseqdesc[m_sequence].bbmax[1];
    maxs[2] = pseqdesc[m_sequence].bbmax[2];
}

void StudioEntity::GetSequenceInfo(float *pflFrameRate, float *pflGroundSpeed)
{
    auto pseqdesc = _model->get<mstudioseqdesc_t>(_model->m_pstudiohdr->seqindex) + (int)m_sequence;

    if (pseqdesc->numframes > 1)
    {
        *pflFrameRate = 256 * pseqdesc->fps / (pseqdesc->numframes - 1);
        *pflGroundSpeed = sqrt(pseqdesc->linearmovement[0] * pseqdesc->linearmovement[0] + pseqdesc->linearmovement[1] * pseqdesc->linearmovement[1] + pseqdesc->linearmovement[2] * pseqdesc->linearmovement[2]);
        *pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
    }
    else
    {
        *pflFrameRate = 256.0;
        *pflGroundSpeed = 0.0;
    }
}

float StudioEntity::SetController(int iController, float flValue)
{
    auto pbonecontroller = _model->get<mstudiobonecontroller_t>(_model->m_pstudiohdr->bonecontrollerindex);

    // find first controller that matches the index
    int i;
    for (i = 0; i < _model->m_pstudiohdr->numbonecontrollers; i++, pbonecontroller++)
    {
        if (pbonecontroller->index == iController)
            break;
    }

    if (i >= _model->m_pstudiohdr->numbonecontrollers)
    {
        return flValue;
    }

    // wrap 0..360 if it's a rotational controller
    if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
    {
        // ugly hack, invert value if end < start
        if (pbonecontroller->end < pbonecontroller->start)
            flValue = -flValue;

        // does the controller not wrap?
        if (pbonecontroller->start + 359.0 >= pbonecontroller->end)
        {
            if (flValue > ((pbonecontroller->start + pbonecontroller->end) / 2.0) + 180)
                flValue = flValue - 360;
            if (flValue < ((pbonecontroller->start + pbonecontroller->end) / 2.0) - 180)
                flValue = flValue + 360;
        }
        else
        {
            if (flValue > 360)
                flValue = flValue - (int)(flValue / 360.0) * 360.0;
            else if (flValue < 0)
                flValue = flValue + (int)((flValue / -360.0) + 1) * 360.0;
        }
    }

    int setting = 255 * (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start);

    if (setting < 0) setting = 0;
    if (setting > 255) setting = 255;
    m_controller[iController] = setting;

    return setting * (1.0 / 255.0) * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}

float StudioEntity::SetMouth(float flValue)
{
    auto pbonecontroller = _model->get<mstudiobonecontroller_t>(_model->m_pstudiohdr->bonecontrollerindex);

    // find first controller that matches the mouth
    for (int i = 0; i < _model->m_pstudiohdr->numbonecontrollers; i++, pbonecontroller++)
    {
        if (pbonecontroller->index == 4)
            break;
    }

    // wrap 0..360 if it's a rotational controller
    if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
    {
        // ugly hack, invert value if end < start
        if (pbonecontroller->end < pbonecontroller->start)
            flValue = -flValue;

        // does the controller not wrap?
        if (pbonecontroller->start + 359.0 >= pbonecontroller->end)
        {
            if (flValue > ((pbonecontroller->start + pbonecontroller->end) / 2.0) + 180)
                flValue = flValue - 360;
            if (flValue < ((pbonecontroller->start + pbonecontroller->end) / 2.0) - 180)
                flValue = flValue + 360;
        }
        else
        {
            if (flValue > 360)
                flValue = flValue - (int)(flValue / 360.0) * 360.0;
            else if (flValue < 0)
                flValue = flValue + (int)((flValue / -360.0) + 1) * 360.0;
        }
    }

    int setting = 64 * (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start);

    if (setting < 0) setting = 0;
    if (setting > 64) setting = 64;
    m_mouth = setting;

    return setting * (1.0 / 64.0) * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}

float StudioEntity::SetBlending(int iBlender, float flValue)
{
    auto pseqdesc = _model->get<mstudioseqdesc_t>(_model->m_pstudiohdr->seqindex) + (int)m_sequence;

    if (pseqdesc->blendtype[iBlender] == 0)
        return flValue;

    if (pseqdesc->blendtype[iBlender] & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
    {
        // ugly hack, invert value if end < start
        if (pseqdesc->blendend[iBlender] < pseqdesc->blendstart[iBlender])
            flValue = -flValue;

        // does the controller not wrap?
        if (pseqdesc->blendstart[iBlender] + 359.0 >= pseqdesc->blendend[iBlender])
        {
            if (flValue > ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) + 180)
                flValue = flValue - 360;
            if (flValue < ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) - 180)
                flValue = flValue + 360;
        }
    }

    int setting = 255 * (flValue - pseqdesc->blendstart[iBlender]) / (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]);

    if (setting < 0) setting = 0;
    if (setting > 255) setting = 255;

    m_blending[iBlender] = setting;

    return setting * (1.0 / 255.0) * (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]) + pseqdesc->blendstart[iBlender];
}

int StudioEntity::SetBodygroup(int iGroup, int iValue)
{
    if (iGroup > _model->m_pstudiohdr->numbodyparts)
    {
        return -1;
    }

    auto pbodypart = _model->get<mstudiobodyparts_t>(_model->m_pstudiohdr->bodypartindex) + iGroup;

    int iCurrent = (m_bodynum / pbodypart->base) % pbodypart->nummodels;

    if (iValue >= pbodypart->nummodels)
    {
        return iCurrent;
    }

    m_bodynum = (m_bodynum - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));

    return iValue;
}

int StudioEntity::SetSkin(int iValue)
{
    if (iValue < _model->m_pstudiohdr->numskinfamilies)
    {
        return m_skinnum;
    }

    m_skinnum = iValue;

    return iValue;
}

void StudioEntity::CalcBoneAdj()
{
    int i, j;
    float value;

    auto pbonecontroller = _model->get<mstudiobonecontroller_t>(_model->m_pstudiohdr->bonecontrollerindex);

    for (j = 0; j < _model->m_pstudiohdr->numbonecontrollers; j++)
    {
        i = pbonecontroller[j].index;
        if (i <= 3)
        {
            // check for 360% wrapping
            if (pbonecontroller[j].type & STUDIO_RLOOP)
            {
                value = m_controller[i] * (360.0 / 256.0) + pbonecontroller[j].start;
            }
            else
            {
                value = m_controller[i] / 255.0;
                if (value < 0) value = 0;
                if (value > 1.0) value = 1.0;
                value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
            }
            // Con_DPrintf( "%d %d %f : %f\n", m_controller[j], m_prevcontroller[j], value, dadt );
        }
        else
        {
            value = m_mouth / 64.0;
            if (value > 1.0) value = 1.0;
            value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
            // Con_DPrintf("%d %f\n", mouthopen, value );
        }
        switch (pbonecontroller[j].type & STUDIO_TYPES)
        {
            case STUDIO_XR:
            case STUDIO_YR:
            case STUDIO_ZR:
                g_adj[j] = value * (Q_PI / 180.0);
                break;
            case STUDIO_X:
            case STUDIO_Y:
            case STUDIO_Z:
                g_adj[j] = value;
                break;
        }
    }
}

void StudioEntity::CalcBoneQuaternion(int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *q)
{
    int j, k;
    vec4_t q1, q2;
    vec3_t angle1, angle2;
    mstudioanimvalue_t *panimvalue;

    for (j = 0; j < 3; j++)
    {
        if (panim->offset[j + 3] == 0)
        {
            angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
        }
        else
        {
            panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j + 3]);
            k = frame;
            while (panimvalue->num.total <= k)
            {
                k -= panimvalue->num.total;
                panimvalue += panimvalue->num.valid + 1;
            }
            // Bah, missing blend!
            if (panimvalue->num.valid > k)
            {
                angle1[j] = panimvalue[k + 1].value;

                if (panimvalue->num.valid > k + 1)
                {
                    angle2[j] = panimvalue[k + 2].value;
                }
                else
                {
                    if (panimvalue->num.total > k + 1)
                        angle2[j] = angle1[j];
                    else
                        angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
                }
            }
            else
            {
                angle1[j] = panimvalue[panimvalue->num.valid].value;
                if (panimvalue->num.total > k + 1)
                {
                    angle2[j] = angle1[j];
                }
                else
                {
                    angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
                }
            }
            angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
            angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
        }

        if (pbone->bonecontroller[j + 3] != -1)
        {
            angle1[j] += g_adj[pbone->bonecontroller[j + 3]];
            angle2[j] += g_adj[pbone->bonecontroller[j + 3]];
        }
    }

    if (!VectorCompare(angle1, angle2))
    {
        AngleQuaternion(angle1, q1);
        AngleQuaternion(angle2, q2);
        QuaternionSlerp(q1, q2, s, q);
    }
    else
    {
        AngleQuaternion(angle1, q);
    }
}

void StudioEntity::CalcBonePosition(int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *pos)
{
    int j, k;
    mstudioanimvalue_t *panimvalue;

    for (j = 0; j < 3; j++)
    {
        pos[j] = pbone->value[j]; // default;
        if (panim->offset[j] != 0)
        {
            panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);

            k = frame;
            // find span of values that includes the frame we want
            while (panimvalue->num.total <= k)
            {
                k -= panimvalue->num.total;
                panimvalue += panimvalue->num.valid + 1;
            }
            // if we're inside the span
            if (panimvalue->num.valid > k)
            {
                // and there's more data in the span
                if (panimvalue->num.valid > k + 1)
                {
                    pos[j] += (panimvalue[k + 1].value * (1.0 - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
                }
                else
                {
                    pos[j] += panimvalue[k + 1].value * pbone->scale[j];
                }
            }
            else
            {
                // are we at the end of the repeating values section and there's another section with data?
                if (panimvalue->num.total <= k + 1)
                {
                    pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
                }
                else
                {
                    pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
                }
            }
        }
        if (pbone->bonecontroller[j] != -1)
        {
            pos[j] += g_adj[pbone->bonecontroller[j]];
        }
    }
}

void StudioEntity::CalcRotations(vec3_t *pos, vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f)
{
    int i;
    int frame;
    float s;

    frame = (int)f;
    s = (f - frame);

    // add in programatic controllers
    CalcBoneAdj();

    auto pbone = _model->get<mstudiobone_t>(_model->m_pstudiohdr->boneindex);

    for (i = 0; i < _model->m_pstudiohdr->numbones; i++, pbone++, panim++)
    {
        CalcBoneQuaternion(frame, s, pbone, panim, q[i]);
        CalcBonePosition(frame, s, pbone, panim, pos[i]);
    }

    if (pseqdesc->motiontype & STUDIO_X)
        pos[pseqdesc->motionbone][0] = 0.0;
    if (pseqdesc->motiontype & STUDIO_Y)
        pos[pseqdesc->motionbone][1] = 0.0;
    if (pseqdesc->motiontype & STUDIO_Z)
        pos[pseqdesc->motionbone][2] = 0.0;
}

mstudioanim_t *StudioEntity::GetAnim(mstudioseqdesc_t *pseqdesc)
{
    auto pseqgroup = _model->get<mstudioseqgroup_t>(_model->m_pstudiohdr->seqgroupindex) + pseqdesc->seqgroup;

    if (pseqdesc->seqgroup == 0)
    {
        return _model->get<mstudioanim_t>(pseqgroup->unused2 /* was pseqgroup->data, will be almost always be 0 */ + pseqdesc->animindex);
    }

    return (mstudioanim_t *)((byte *)_model->m_panimhdr[pseqdesc->seqgroup] + pseqdesc->animindex);
}

void StudioEntity::SlerpBones(vec4_t q1[], vec3_t pos1[], vec4_t q2[], vec3_t pos2[], float s)
{
    int i;
    vec4_t q3;
    float s1;

    if (s < 0)
        s = 0;
    else if (s > 1.0)
        s = 1.0;

    s1 = 1.0 - s;

    for (i = 0; i < _model->m_pstudiohdr->numbones; i++)
    {
        QuaternionSlerp(q1[i], q2[i], s, q3);
        q1[i][0] = q3[0];
        q1[i][1] = q3[1];
        q1[i][2] = q3[2];
        q1[i][3] = q3[3];
        pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
        pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
        pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
    }
}

void StudioEntity::AdvanceFrame(float dt)
{
    auto pseqdesc = _model->get<mstudioseqdesc_t>(_model->m_pstudiohdr->seqindex) + m_sequence;

    if (dt > 0.1)
        dt = (float)0.1;
    m_frame += dt * pseqdesc->fps;

    if (pseqdesc->numframes <= 1)
    {
        m_frame = 0;
    }
    else
    {
        // wrap
        m_frame -= (int)(m_frame / (pseqdesc->numframes - 1)) * (pseqdesc->numframes - 1);
    }
}

void StudioEntity::SetUpBones(void)
{
    static vec3_t pos[MAXSTUDIOBONES];
    float bonematrix[4][4];
    static vec4_t q[MAXSTUDIOBONES];

    if (m_sequence >= _model->m_pstudiohdr->numseq)
    {
        m_sequence = 0;
    }

    auto pseqdesc = _model->get<mstudioseqdesc_t>(_model->m_pstudiohdr->seqindex) + m_sequence;

    auto panim = GetAnim(pseqdesc);
    CalcRotations(pos, q, pseqdesc, panim, m_frame);

    if (pseqdesc->numblends > 1)
    {
        static vec3_t pos2[MAXSTUDIOBONES];
        static vec4_t q2[MAXSTUDIOBONES];

        float s;

        panim += _model->m_pstudiohdr->numbones;
        CalcRotations(pos2, q2, pseqdesc, panim, m_frame);
        s = m_blending[0] / 255.0;

        SlerpBones(q, pos, q2, pos2, s);

        if (pseqdesc->numblends == 4)
        {
            static vec3_t pos3[MAXSTUDIOBONES];
            static vec4_t q3[MAXSTUDIOBONES];
            static vec3_t pos4[MAXSTUDIOBONES];
            static vec4_t q4[MAXSTUDIOBONES];

            panim += _model->m_pstudiohdr->numbones;
            CalcRotations(pos3, q3, pseqdesc, panim, m_frame);

            panim += _model->m_pstudiohdr->numbones;
            CalcRotations(pos4, q4, pseqdesc, panim, m_frame);

            s = m_blending[0] / 255.0;
            SlerpBones(q3, pos3, q4, pos4, s);

            s = m_blending[1] / 255.0;
            SlerpBones(q, pos, q3, pos3, s);
        }
    }

    auto pbones = _model->get<mstudiobone_t>(_model->m_pstudiohdr->boneindex);

    for (int i = 0; i < _model->m_pstudiohdr->numbones; i++)
    {
        QuaternionMatrix(q[i], bonematrix);

        bonematrix[0][3] = pos[i][0];
        bonematrix[1][3] = pos[i][1];
        bonematrix[2][3] = pos[i][2];
        bonematrix[3][3] = 0.0f;

        if (pbones[i].parent == -1)
        {
            memcpy(g_bonetransform[i], bonematrix, sizeof(float) * 16);
        }
        else
        {
            R_ConcatTransforms(g_bonetransform[pbones[i].parent], bonematrix, g_bonetransform[i]);
        }
    }
}

void StudioEntity::Lighting(float *lv, int bone, int flags, vec3_t normal)
{
    float illum;
    float lightcos;

    illum = g_ambientlight;

    if (flags & STUDIO_NF_FLATSHADE)
    {
        illum += g_shadelight * 0.8;
    }
    else
    {
        float r;
        lightcos = DotProduct(normal, g_blightvec[bone]); // -1 colinear, 1 opposite

        if (lightcos > 1.0)
        {
            lightcos = 1;
        }

        illum += g_shadelight;

        r = g_lambert;
        if (r <= 1.0) r = 1.0;

        lightcos = (lightcos + (r - 1.0)) / r; // do modified hemispherical lighting
        if (lightcos > 0.0)
        {
            illum -= g_shadelight * lightcos;
        }
        if (illum <= 0)
        {
            illum = 0;
        }
    }

    if (illum > 255)
    {
        illum = 255;
    }

    *lv = illum / 255.0; // Light from 0 to 1.0
}

void StudioEntity::Chrome(int *pchrome, int bone, vec3_t normal)
{
    float n;

    if (g_chromeage[bone] != g_smodels_total)
    {
        // calculate vectors from the viewer to the bone. This roughly adjusts for position
        vec3_t chromeupvec;    // g_chrome t vector in world reference frame
        vec3_t chromerightvec; // g_chrome s vector in world reference frame
        vec3_t tmp;            // vector pointing at bone in world reference frame
        vec3_t origin;
        VectorScale(origin, -1, tmp);
        tmp[0] += g_bonetransform[bone][0][3];
        tmp[1] += g_bonetransform[bone][1][3];
        tmp[2] += g_bonetransform[bone][2][3];
        VectorNormalize(tmp);
        CrossProduct(tmp, g_vright, chromeupvec);
        VectorNormalize(chromeupvec);
        CrossProduct(tmp, chromeupvec, chromerightvec);
        VectorNormalize(chromerightvec);

        VectorIRotate(chromeupvec, g_bonetransform[bone], g_chromeup[bone]);
        VectorIRotate(chromerightvec, g_bonetransform[bone], g_chromeright[bone]);

        g_chromeage[bone] = g_smodels_total;
    }

    // calc s coord
    n = DotProduct(normal, g_chromeright[bone]);
    pchrome[0] = (n + 1.0) * 32; // FIX: make this a float

    // calc t coord
    n = DotProduct(normal, g_chromeup[bone]);
    pchrome[1] = (n + 1.0) * 32; // FIX: make this a float
}

/*
================
StudioEntity::SetupLighting
	set some global variables based on entity position
inputs:
outputs:
	g_ambientlight
	g_shadelight
================
*/
void StudioEntity::SetupLighting()
{
    int i;
    g_ambientlight = 32;
    g_shadelight = 192;

    g_lightvec[0] = 0;
    g_lightvec[1] = 0;
    g_lightvec[2] = -1.0;

    g_lightcolor[0] = 1.0;
    g_lightcolor[1] = 1.0;
    g_lightcolor[2] = 1.0;

    // TODO: only do it for bones that actually have textures
    for (i = 0; i < _model->m_pstudiohdr->numbones; i++)
    {
        VectorIRotate(g_lightvec, g_bonetransform[i], g_blightvec[i]);
    }
}

/*
=================
StudioEntity::SetupModel
	based on the body part, figure out which mesh it should be using.
inputs:
	currententity
outputs:
	pstudiomesh
	pmdl
=================
*/
void StudioEntity::SetupModel(int bodypart)
{
    int index;

    if (bodypart > _model->m_pstudiohdr->numbodyparts)
    {
        // Con_DPrintf ("StudioEntity::SetupModel: no such bodypart %d\n", bodypart);
        bodypart = 0;
    }

    auto pbodypart = _model->get<mstudiobodyparts_t>(_model->m_pstudiohdr->bodypartindex) + bodypart;

    index = m_bodynum / pbodypart->base;
    index = index % pbodypart->nummodels;

    _model->m_pmodel = _model->get<mstudiomodel_t>(pbodypart->modelindex) + index;
}

/*
================
StudioEntity::DrawModel
inputs:
	currententity
	r_entorigin
================
*/
void StudioEntity::DrawModel(RenderApi &renderer)
{
    int i;

    g_smodels_total++; // render data cache cookie

    g_pxformverts = &g_xformverts[0];
    g_pvlightvalues = &g_lightvalues[0];

    if (_model->m_pstudiohdr->numbodyparts == 0)
        return;

    SetUpBones();

    for (i = 0; i < _model->m_pstudiohdr->numbodyparts; i++)
    {
        SetupModel(i);
        DrawPoints(renderer);
    }
}

//#define LEGACY_CODE 1
/*
================

================
*/
void StudioEntity::DrawPoints(RenderApi &renderer)
{
    int i;

    auto pvertbone = _model->get<byte>(_model->m_pmodel->vertinfoindex);
    auto ptexture = _model->get_texture<mstudiotexture_t>(_model->m_ptexturehdr->textureindex);

    auto pstudioverts = _model->get<vec3_t>(_model->m_pmodel->vertindex);

    auto pskinref = _model->get<short>(_model->m_ptexturehdr->skinindex);
    if (m_skinnum != 0 && m_skinnum < _model->m_ptexturehdr->numskinfamilies)
        pskinref += (m_skinnum * _model->m_ptexturehdr->numskinref);

    renderer.SetupBones(g_bonetransform, _model->m_pstudiohdr->numbones);

    for (i = 0; i < _model->m_pmodel->numverts; i++)
    {
        /*
        VectorTransform(pstudioverts[i], g_bonetransform[pvertbone[i]], g_pxformverts[i]);
        /*/
        for (int w = 0; w < 3; w++)
            g_pxformverts[i][w] = pstudioverts[i][w];
//*/
    }

    glCullFace(GL_FRONT);

    for (int j = 0; j < _model->m_pmodel->nummesh; j++)
    {
        float s, t;
        short *ptricmds;

        auto pmesh = _model->get<mstudiomesh_t>(_model->m_pmodel->meshindex) + j;
        ptricmds = _model->get<short>(pmesh->triindex);

        s = 1.0 / (float)ptexture[pskinref[pmesh->skinref]].width;
        t = 1.0 / (float)ptexture[pskinref[pmesh->skinref]].height;

#ifdef LEGACY_CODE
        glBindTexture(GL_TEXTURE_2D, ptexture[pskinref[pmesh->skinref]].index);
#else
        renderer.BeginMesh();
        renderer.Texture(ptexture[pskinref[pmesh->skinref]].index);
#endif // LEGACY_CODE

        bool useChrome = ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_CHROME;

        while (i = *(ptricmds++))
        {
            if (i < 0)
            {
#ifdef LEGACY_CODE
                glBegin(GL_TRIANGLE_FAN);
#else
                renderer.BeginFace(true);
#endif // LEGACY_CODE
                i = -i;
            }
            else
            {
#ifdef LEGACY_CODE
                glBegin(GL_TRIANGLE_STRIP);
#else
                renderer.BeginFace(false);
#endif // LEGACY_CODE
            }

            for (; i > 0; i--, ptricmds += 4)
            {
                renderer.Bone(pvertbone[ptricmds[1]]);
                if (useChrome)
                {
                    // FIX: put these in as integer coords, not floats
#ifdef LEGACY_CODE
                    glTexCoord2f(g_chrome[ptricmds[1]][0] * s, g_chrome[ptricmds[1]][1] * t);
#else
                    renderer.Uv(glm::vec2(g_chrome[ptricmds[1]][0] * s, g_chrome[ptricmds[1]][1] * t));
#endif // LEGACY_CODE
                }
                else
                {
                    // FIX: put these in as integer coords, not floats
#ifdef LEGACY_CODE
                    glTexCoord2f(ptricmds[2] * s, ptricmds[3] * t);
#else
                    renderer.Uv(glm::vec2(ptricmds[2] * s, ptricmds[3] * t));
#endif // LEGACY_CODE
                }

#ifdef LEGACY_CODE
                glColor4f(1.0, 1.0, 1.0, 1.0);
#else
#endif // LEGACY_CODE

                auto av = g_pxformverts[ptricmds[0]];
#ifdef LEGACY_CODE
                glVertex3f(av[0], av[1], av[2]);
#else
                renderer.Position(glm::vec3(av[0], av[1], av[2]));
#endif // LEGACY_CODE
            }
#ifdef LEGACY_CODE
            glEnd();
#else
            renderer.EndFace();
#endif // LEGACY_CODE
        }
#ifndef LEGACY_CODE
        renderer.EndMesh();
#endif // LEGACY_CODE
    }
}
