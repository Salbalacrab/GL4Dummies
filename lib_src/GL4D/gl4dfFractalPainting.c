/*!\file gl4dfFractalPainting.c
 *
 * \brief Filtre de painting ou de génération de modèle numérique de
 * terrain (MNT ou DEM) sous contraintes à partir d'une texture ou
 * l'écran vers une texture ou l'écran. Ceci correpond à une
 * Implémentation complète du MCMD en gpu. Ce pipeline est capable de
 * calculer un MCMD indépendamment sur chacune des 4 composantes de
 * couleur. Voir "Farès Belhadj. Modélisation automatique de
 * géo-environnements naturels et multi-urbains.  Thèse de doctorat en
 * Informatique de l'Université Paris 8, soutenue le 11 décembre
 * 2007."
 *
 * \author Farès BELHADJ amsi@ai.univ-paris8.fr
 * \date december 2010 - october 25, 2017
 * \todo remplacer l'usage de la liste chainÉe par quelque chose de
 * plus efficace.
*/
#include <assert.h>
#include <GL4D/gl4du.h>
#include <GL4D/gl4df.h>
#include <GL4D/gl4dfCommon.h>

static inline int nbLevels(int w, int h);
static int  mdTexData(unsigned int w, unsigned int h);
static void init(void);
static void quit(void);

static GLuint _pId[4] = { 0 }, _mdbu_version = 1 + 2;
static GLuint _mdTexId[4] = { 0 }, _buTreeSize = 0,  _tempTexId[3] = { 0 };
static GLuint _width = 512, _height = 512;
static int    _maxLevel = -1;
static GLfloat _rand_threshold = 1.0f, _seed = 0.0f;
static GLboolean _skeletonize = GL_FALSE, _change_seed = GL_FALSE, _mcmd_take_color = GL_FALSE;
static GLfloat _mcmd_Ir[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static GLfloat _mcmd_noise_H[4] = { 0.5f, 0.5f, 0.5f, 0.5f }, 
  _mcmd_noise_S[4] = { 1.0f, 1.0f, 1.0f, 1.0f }, 
  _mcmd_noise_T[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, 
  _mcmd_noise_phase_change[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, 
  _mcmd_I[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

MKFWINIT3(fractalPainting, void, GLuint, GLuint, GLboolean);

void gl4dfMCMD(GLuint in, GLuint out, GLboolean flipV) {
  fractalPaintingfptr(in, out, flipV);
}

void gl4dfMCMDDimensions(GLuint width, GLuint height) {
  int d = (int)ceil(log(width) / log(2.0));
  if(width != (1 << d)) {
    fprintf(stderr, 
	    "%s:%d: les dimensions du MCMD sont des puissances de 2. Conversion a la puissance de 2 superieure.\n", 
	    __FILE__, __LINE__);
    width = (1 << d);
  }
  d = (int)ceil(log(height) / log(2.0));
  if(height != (1 << d)) {
    fprintf(stderr, 
	    "%s:%d: les dimensions du MCMD sont des puissances de 2. Conversion a la puissance de 2 superieure.\n", 
	    __FILE__, __LINE__);
    height = (1 << d);
  }
  _width = width;
  _height = height;
  init();
  fractalPaintingfptr = fractalPaintingffunc;
}

static void fractalPaintingfinit(GLuint in, GLuint out, GLboolean flipV) {
  init();
  fractalPaintingfptr = fractalPaintingffunc;
  fractalPaintingfptr(in, out, flipV);
}

static void fractalPaintingffunc(GLuint in, GLuint out, GLboolean flipV) {
  GLint i, ati = 0, vp[4], polygonMode[2], cpId = 0, cfbo, end;
  GLboolean dt = glIsEnabled(GL_DEPTH_TEST), bl = glIsEnabled(GL_BLEND), tex = glIsEnabled(GL_TEXTURE_2D);
  GLuint fbo;
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glGetIntegerv(GL_POLYGON_MODE, polygonMode);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glGetIntegerv(GL_VIEWPORT, vp);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cfbo);
  glGetIntegerv(GL_CURRENT_PROGRAM, &cpId);
  glGenFramebuffers(1, &fbo);
  gl4dfConvSetFilter(GL_NEAREST);
  if(_skeletonize) {
    if(in == 0) { /* Pas d'entrée, donc l'entrée est le dernier draw */
      gl4dfConvFrame2Tex(&_tempTexId[2]);
    } else
      gl4dfConvTex2Tex(in, _tempTexId[2], GL_FALSE);
  } else { 
    if(in == 0) { /* Pas d'entrée, donc l'entrée est le dernier draw */
      gl4dfConvFrame2Tex(&_tempTexId[1]);
    } else
      gl4dfConvTex2Tex(in, _tempTexId[1], GL_FALSE);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); {
    glViewport(0, 0, _width, _height);
    if(_skeletonize) {
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _tempTexId[1],  0);
      glUseProgram(_pId[1]);
      glUniform1f(glGetUniformLocation(_pId[1], "rand_threshold"), _rand_threshold);
      glUniform1i(glGetUniformLocation(_pId[1], "etage0"), 0);
      glUniform1i(glGetUniformLocation(_pId[1], "inv"), 0);
      glUniform1i(glGetUniformLocation(_pId[1], "width"), _width);
      glUniform1i(glGetUniformLocation(_pId[1], "height"), _height);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, _tempTexId[2]);
      gl4dgDraw(fcommGetPlane());
    }
    /* mdbu */
    glUseProgram(_pId[_mdbu_version]);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, _tempTexId[2]);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, _mdTexId[3]);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, _mdTexId[2]);
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "etage0"), 0);
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "etage1"), 1);
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "etage2"), 2);
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "etage3"), 3);
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "width"), _width);
    glUniform4fv(glGetUniformLocation(_pId[_mdbu_version], "mcmd_Ir"), 1, _mcmd_Ir);
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "mcmd_take_color"), _mcmd_take_color);
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "buTreeSize"), _buTreeSize);
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "buTreeWidth"), (int)sqrt(_buTreeSize));
    glUniform1i(glGetUniformLocation(_pId[_mdbu_version], "buTreeHeight"), _buTreeSize / (int)sqrt(_buTreeSize));
    /* end = _mdbu_version > 2 ? (nbLevels(_width, _height) >> 2) - 1 : nbLevels(_width, _height) - 1; */
    end = (nbLevels(_width, _height) >> 2) - 1;
    for(i = 0, ati = 0; i < end; i++) {
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,    GL_TEXTURE_2D, _tempTexId[ati], 0);
      ati = (ati + 1) % 2;
      glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, _tempTexId[ati]);
      gl4dgDraw(fcommGetPlane());
    }
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0); 
    /* fin mdbu */
    /* debut md */
    glUseProgram(_pId[0]);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, _mdTexId[1]);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, _mdTexId[0]);
    glUniform1i(glGetUniformLocation(_pId[0], "etage0"), 0);
    glUniform1i(glGetUniformLocation(_pId[0], "etage1"), 1);
    glUniform1i(glGetUniformLocation(_pId[0], "etage2"), 2);
    glUniform1i(glGetUniformLocation(_pId[0], "width"), _width);
    glUniform1i(glGetUniformLocation(_pId[0], "height"), _height);
    glUniform1i(glGetUniformLocation(_pId[0], "maxLevel"), _maxLevel);
    glUniform4fv(glGetUniformLocation(_pId[0], "mcmd_noise_H"), 1, _mcmd_noise_H);
    glUniform4fv(glGetUniformLocation(_pId[0], "mcmd_noise_S"), 1, _mcmd_noise_S);
    glUniform4fv(glGetUniformLocation(_pId[0], "mcmd_noise_T"), 1, _mcmd_noise_T);
    glUniform4fv(glGetUniformLocation(_pId[0], "mcmd_noise_phase_change"), 1, _mcmd_noise_phase_change);
    glUniform4fv(glGetUniformLocation(_pId[0], "mcmd_I"), 1, _mcmd_I);
    glUniform1f(glGetUniformLocation(_pId[0], "seed"), _seed);
    if(_change_seed)
      _seed += 0.0001f;
    for(i = 0; i < nbLevels(_width, _height); i++) {
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,    GL_TEXTURE_2D, _tempTexId[ati], 0);
      ati = (ati + 1) % 2;
      glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, _tempTexId[ati]);
      glUniform1i(glGetUniformLocation(_pId[0], "level"), i);
      gl4dgDraw(fcommGetPlane());
    }
    /* fin md */
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
  }
  /* fin: */
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  if(!out) { /* Copier à l'écran en cas de out nul */
    glUseProgram(0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    if(flipV)
      glBlitFramebuffer(0, 0, _width, _height, vp[0], vp[1] + vp[3], vp[0] + vp[2], vp[1], GL_COLOR_BUFFER_BIT, GL_LINEAR);
    else
      glBlitFramebuffer(0, 0, _width, _height, vp[0], vp[1], vp[0] + vp[2], vp[1] + vp[3], GL_COLOR_BUFFER_BIT, GL_LINEAR);
  } else
    gl4dfConvTex2Tex(_tempTexId[(ati + 1) % 2], out, flipV);
  glDeleteFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)cfbo);
  glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
  glUseProgram(cpId);
  if(dt) glEnable(GL_DEPTH_TEST);
  if(bl) glEnable(GL_BLEND);
  if(!tex) glDisable(GL_TEXTURE_2D);

}

void gl4dfMCMDTakeColor(GLboolean mcmd_take_color) {
  _mcmd_take_color = mcmd_take_color;
}

void gl4dfMCMDChangeSeed(GLboolean change_seed) {
  _change_seed = change_seed;
}

void gl4dfMCMDSetSkeletonize(GLboolean skeletonize) {
  _skeletonize = skeletonize;
}

void gl4dfMCMDSetIr(const GLfloat * mcmd_Ir) {
  memcpy(_mcmd_Ir, mcmd_Ir, sizeof _mcmd_Ir);
}

void gl4dfMCMDSetI(const GLfloat * mcmd_I) {
  memcpy(_mcmd_I, mcmd_I, sizeof _mcmd_I);
}

void gl4dfMCMDSetNoiseH(const GLfloat * mcmd_noise_H) {
  memcpy(_mcmd_noise_H, mcmd_noise_H, sizeof _mcmd_noise_H);
}

void gl4dfMCMDSetNoiseS(const GLfloat * mcmd_noise_S) {
  memcpy(_mcmd_noise_S, mcmd_noise_S, sizeof _mcmd_noise_S);
}

void gl4dfMCMDSetNoiseT(const GLfloat * mcmd_noise_T) {
  memcpy(_mcmd_noise_T, mcmd_noise_T, sizeof _mcmd_noise_T);
}

void gl4dfMCMDSetNoisePhaseChange(const GLfloat * mcmd_noise_phase_change) {
  memcpy(_mcmd_noise_phase_change, mcmd_noise_phase_change, sizeof _mcmd_noise_phase_change);
}

void gl4dfMCMDSetSkeletonizeRandThreshold(GLfloat rand_threshold) {
  _rand_threshold = rand_threshold;
}

void gl4dfMCMDSetMDBUVersion(GLuint version) {
  _mdbu_version = MIN(version, 1) + 2;
}

static const char * gl4dfMCMD_select4mcmdFS = 
  "<imfs>gl4dfMCMD_select4mcmd.fs</imfs>\n\
     #version 330\n							\
     in vec2 vsoTexCoord;\n						\
     out vec4 fragColor;\n						\
     uniform int width, height;\n					\
     uniform sampler2D etage0;\n					\
     uniform float rand_threshold;\n					\
     const int ossize = 9;\n						\
     const vec2 G[ossize] = vec2[]( vec2(1.0,  1.0), vec2(0.0,  2.0), vec2(-1.0,  1.0), \n \
     			       vec2(2.0,  0.0), vec2(0.0,  0.0), vec2(-2.0,  0.0), \n \
     			       vec2(1.0, -1.0), vec2(0.0, -2.0), vec2(-1.0, -1.0) );\n \
     vec2 step = vec2(1.0 / float(width - 1), 1.0 / float(height - 1));\n \
     vec2 offset[ossize] = vec2[](vec2(-step.x , -step.y), vec2( 0.0, -step.y), vec2( step.x , -step.y), \n \
     			     vec2(-step.x, 0.0),       vec2( 0.0, 0.0),      vec2( step.x, 0.0), \n \
     			     vec2(-step.x,   step.y), vec2( 0.0, step.y),  vec2( step.x ,  step.y));\n \
     float luminance(vec3 rgb) {\n					\
       return dot(vec3(0.299, 0.587, 0.114), rgb);\n			\
     }\n								\
     highp float rand(vec2 co) {\n					\
       highp float a = 12.9898;\n					\
       highp float b = 78.233;\n					\
       highp float c = 43758.5453;\n					\
       highp float dt= dot(co.xy ,vec2(a,b));\n				\
       highp float sn= mod(dt,3.14);\n					\
       return fract(sin(sn) * c);\n					\
     }\n								\
     float sobel(void) {\n						\
       vec2 g = vec2(0.0, 0.0);\n					\
       for(int i = 0; i < ossize; i++)\n				\
         g += luminance(texture(etage0, vsoTexCoord.st + offset[i]).rgb) * G[i];\n \
       return 1.0 - length(g);\n					\
     }\n								\
     float sobelFromMedia(void) {\n					\
       vec2 g = vec2(0.0, 0.0);\n					\
       for(int i = 0; i < ossize; i++)\n				\
         g += pow(texture(etage0, vsoTexCoord.st + offset[i]).a, 2.99) * G[i];\n \
       return 1.0 - length(g);\n					\
     }\n								\
     const vec3 minc = vec3(1.0 / 255.0);\n				\
     void main() {\n							\
       if((rand(vsoTexCoord.st) > rand_threshold) || 0.2 * sobel() < 0.15 || sobelFromMedia() < 0.1)\n \
         fragColor = vec4(max(texture(etage0, vsoTexCoord.st).rgb, minc), 1.0);\n \
       else\n								\
         fragColor = vec4(0.0, 0.0, 0.0, 1.0);\n			\
     }";

static const char * gl4dfMCMD_mdFS = 
  "<imfs>gl4dfMCMD_md.fs</imfs>\n\
     #version 330\n							\
     in vec2 vsoTexCoord;\n						\
     out vec4 fragColor;\n						\
     uniform int width, height, level, maxLevel;\n			\
     uniform sampler2D etage0, etage1, etage2;\n			\
     uniform float seed;\n						\
     uniform vec4 mcmd_noise_H, mcmd_noise_S, mcmd_noise_T, mcmd_noise_phase_change, mcmd_I;\n \
     vec2[4] getParents(vec2 st) {\n					\
       int i, x;\n							\
       vec2 p[4];\n							\
       vec4 c;\n							\
       for(i = 0; i < 4; i++) {\n					\
         x = i + int((st.s * float(width) - 0.5 /*transforme le nearest en floor*/) * 4.0);\n \
         c = texture(etage1, vec2(float(x) / (float(width) * 4.0), st.t));\n \
         p[i].x = (int(c.r * 255.0) << 8) | int(c.g * 255.0);\n		\
         p[i].y = (int(c.b * 255.0) << 8) | int(c.a * 255.0);\n		\
       }\n								\
       return p;\n							\
     }\n								\
     highp float rand(vec2 co) {\n					\
       highp float a = 12.9898;\n					\
       highp float b = 78.233;\n					\
       highp float c = 43758.5453;\n					\
       highp float dt= dot(co.xy ,vec2(a,b));\n				\
       highp float sn= mod(dt,3.14);\n					\
       return 2.0 * fract(sin(sn) * c) - 1.0;\n				\
     }\n								\
     highp vec4 rand(vec2 co, vec4 s) {\n				\
       return vec4(rand(co + vec2(s.x)), rand(co + vec2(s.y)), rand(co + vec2(s.z)), rand(co + vec2(s.w)));\n \
     }\n								\
     void main() {\n							\
       const vec4 minc = vec4(1.0 / 255.0);\n				\
       int myLevel = int(texture(etage2, vsoTexCoord.st).r * 255.0);\n	\
       if(myLevel == level && texture(etage0, vsoTexCoord.st).r == 0.0) {\n \
         const float maxdist = sqrt(2.0);\n				\
         float sumw = 0.0, w;\n						\
         vec4 I_sign = vec4(mcmd_I.x < 0.0 ? -1.0 : 1.0, mcmd_I.y < 0.0 ? -1.0 : 1.0, mcmd_I.z < 0.0 ? -1.0 : 1.0, mcmd_I.w < 0.0 ? -1.0 : 1.0);\n \
         vec4 I_abs = abs(mcmd_I), v = vec4(0.0);\n			\
         vec2[4] parents = getParents(vsoTexCoord.st);\n		\
         for(int i = 0; i < 4; i++) {\n					\
           if(parents[i].x != 65535) {\n				\
             vec2 p = vec2(parents[i].x / width , parents[i].y / height);\n \
             w = 1.0 - length(p - vsoTexCoord) / maxdist;\n		\
             v += w * (vec4(1.0) - I_sign * (vec4(1.0) - pow(vec4(w), I_abs))) * texture(etage0, p);\n \
             sumw += w;\n						\
           }\n								\
         }\n								\
         if(sumw > 0.0)\n						\
           v /= sumw;\n							\
         v += pow(vec4(2.0), vec4(-(myLevel + 1) * mcmd_noise_H)) * (mcmd_noise_T + mcmd_noise_S * rand(vsoTexCoord.st + vec2(seed), mcmd_noise_phase_change));\n \
         fragColor = max(v, minc);\n					\
       } else\n								\
         fragColor = texture(etage0, vsoTexCoord.st);\n			\
     }";

static const char * gl4dfMCMD_mdbuV0FS = 
  "<imfs>gl4dfMCMD_mdbuV0.fs</imfs>\n\
     #version 330\n							\
     in vec2 vsoTexCoord;\n						\
     out vec4 fragColor;\n						\
     uniform vec4 mcmd_Ir;\n						\
     uniform int width, mcmd_take_color, buTreeSize, buTreeWidth, buTreeHeight;\n \
     uniform sampler2D etage0, etage1, etage2, etage3;\n		\
     float luminance(vec3 rgb) {\n					\
       return dot(vec3(0.299, 0.587, 0.114), rgb);\n			\
     }\n								\
     uint rgba2ui(vec4 rgba) {\n					\
       return (uint(rgba.r * 255.0) << uint(24)) |\n			\
         (uint(rgba.g * 255.0) << uint(16)) |\n				\
         (uint(rgba.b * 255.0) << uint(8) ) |\n				\
         (uint(rgba.a * 255.0));\n					\
     }\n								\
     float rgba2f(vec4 rgba) {\n					\
       return float(rgba2ui(rgba)) / float(uint(-1));\n			\
     }\n								\
     vec2 getChild(uint ipos) {\n					\
       return vec2(float(ipos % uint(buTreeWidth)) / float(buTreeWidth - 1.0), float(ipos / uint(buTreeWidth)) / float(buTreeHeight - 1.0));\n \
     }\n								\
     vec2 readChildCoords(uint pos, float step) {\n			\
       return vec2(texture(etage2, getChild(pos)).r, texture(etage2, getChild(pos + uint(1))).r) * step;\n \
     }\n								\
     void main() {\n							\
       vec4 coul = texture(etage0, vsoTexCoord.st);\n			\
       if(!(coul.r == 0.0 && coul.g == 0.0 && coul.b == 0.0)) {\n	\
         fragColor = coul;\n						\
         return;\n							\
       }\n								\
       float step = 65535.0 / float(width - 1.0);\n			\
       uint i0 = rgba2ui(texture(etage1, vsoTexCoord.st));\n		\
       if(mcmd_take_color != 0) {\n					\
         uint i0 = rgba2ui(texture(etage1, vsoTexCoord.st));\n		\
         for(vec2 ret; (ret = readChildCoords(i0, step)).x <= 1.0; i0 += uint(2)) {\n \
           if(ret.x >= 0.0 && ret.x <= 1.0 && ret.y >= 0.0 && ret.y <= 1.0) {\n	\
             coul = texture(etage0, ret);\n				\
     	     if(!(coul.r == 0.0 && coul.g == 0.0 && coul.b == 0.0)) {\n	\
               fragColor = texture(etage3, vsoTexCoord.st);\n		\
               return;\n						\
             }\n							\
           }\n								\
         }\n								\
         fragColor = vec4(0.0, 0.0, 0.0, 1.0);\n			\
       } else {\n							\
         const float maxdist = sqrt(2.0);\n				\
         float sumn = 0.0, w;\n						\
         vec4 Ir_sign = vec4(mcmd_Ir.x < 0.0 ? -1.0 : 1.0, mcmd_Ir.y < 0.0 ? -1.0 : 1.0, mcmd_Ir.z < 0.0 ? -1.0 : 1.0, mcmd_Ir.w < 0.0 ? -1.0 : 1.0);\n	\
         vec4 sumcoul = vec4(0.0), Ir_abs = abs(mcmd_Ir);\n		\
         for(vec2 ret; (ret = readChildCoords(i0, step)).x <= 1.0; i0 += uint(2)) {\n \
           if(ret.x >= 0.0 && ret.x <= 1.0 && ret.y >= 0.0 && ret.y <= 1.0) {\n	\
             coul = texture(etage0, ret);\n				\
             if(!(coul.r == 0.0 && coul.g == 0.0 && coul.b == 0.0)) {\n	\
               w = 1.0 - length(ret - vsoTexCoord) / maxdist;\n		\
               sumcoul += w * (vec4(1.0) - Ir_sign * (vec4(1.0) - pow(vec4(w), Ir_abs))) * coul; \
               sumn += w;\n						\
             }\n							\
           }\n								\
         }\n								\
         if(sumn > 0.0)\n						\
           fragColor = sumcoul / sumn;\n				\
         else\n								\
           fragColor = vec4(0.0, 0.0, 0.0, 1.0);\n			\
       }\n								\
     }";

static const char * gl4dfMCMD_mdbuV1FS = 
  "<imfs>gl4dfMCMD_mdbuV1.fs</imfs>\n\
     #version 330\n							\
     in vec2 vsoTexCoord;\n						\
     out vec4 fragColor;\n						\
     uniform vec4 mcmd_Ir;\n						\
     uniform int width, mcmd_take_color, buTreeSize, buTreeWidth, buTreeHeight;\n \
     uniform sampler2D etage0, etage1, etage2, etage3;\n		\
     float luminance(vec3 rgb) {\n					\
       return dot(vec3(0.299, 0.587, 0.114), rgb);\n			\
     }\n								\
     uint rgba2ui(vec4 rgba) {\n					\
       return (uint(rgba.r * 255.0) << uint(24)) |\n			\
         (uint(rgba.g * 255.0) << uint(16)) |\n				\
         (uint(rgba.b * 255.0) << uint(8) ) |\n				\
         (uint(rgba.a * 255.0));\n					\
     }\n								\
     float rgba2f(vec4 rgba) {\n					\
       return float(rgba2ui(rgba)) / float(uint(-1));\n			\
     }\n								\
     vec2 getChild(uint ipos) {\n					\
       return vec2(float(ipos % uint(buTreeWidth)) / float(buTreeWidth - 1.0), float(ipos / uint(buTreeWidth)) / float(buTreeHeight - 1.0));\n \
     }\n								\
     vec2 readChildCoords(uint pos, float step) {\n			\
       return vec2(texture(etage2, getChild(pos)).r, texture(etage2, getChild(pos + uint(1))).r) * step;\n \
     }\n								\
     void main() {\n							\
       vec4 coul = texture(etage0, vsoTexCoord.st);\n			\
       if(!(coul.r == 0.0 && coul.g == 0.0 && coul.b == 0.0)) {\n	\
         fragColor = coul;\n						\
         return;\n							\
       }\n								\
       float step = 65535.0 / float(width - 1.0);\n			\
       uint i0 = rgba2ui(texture(etage1, vsoTexCoord.st));\n		\
       if(mcmd_take_color != 0) {\n					\
         uint i0 = rgba2ui(texture(etage1, vsoTexCoord.st));\n		\
         for(vec2 ret; (ret = readChildCoords(i0, step)).x <= 1.0; i0 += uint(2)) {\n \
           if(ret.x >= 0.0 && ret.x <= 1.0 && ret.y >= 0.0 && ret.y <= 1.0) {\n	\
             coul = texture(etage0, ret);\n				\
     	     if(!(coul.r == 0.0 && coul.g == 0.0 && coul.b == 0.0)) {\n	\
               fragColor = texture(etage3, vsoTexCoord.st);\n		\
               return;\n						\
             }\n							\
           }\n								\
         }\n								\
         fragColor = vec4(0.0, 0.0, 0.0, 1.0);\n			\
       } else {\n							\
         const float maxdist = sqrt(2.0);\n				\
         float sumn = 0.0, w, d, sc = 0.032, sf = 0.01;\n		\
         vec4 Ir_sign = vec4(mcmd_Ir.x < 0.0 ? -1.0 : 1.0, mcmd_Ir.y < 0.0 ? -1.0 : 1.0, mcmd_Ir.z < 0.0 ? -1.0 : 1.0, mcmd_Ir.w < 0.0 ? -1.0 : 1.0);\n	\
         vec4 sumcoul = vec4(0.0), Ir_abs = abs(mcmd_Ir);\n		\
         for(vec2 ret; (ret = readChildCoords(i0, step)).x <= 1.0; i0 += uint(2)) {\n \
           if(ret.x >= 0.0 && ret.x <= 1.0 && ret.y >= 0.0 && ret.y <= 1.0) {\n	\
             coul = texture(etage0, ret);\n				\
             if(!(coul.r == 0.0 && coul.g == 0.0 && coul.b == 0.0)) {\n	\
               d = (length(ret - vsoTexCoord) / maxdist);\n		\
               //sf=0.1;sc=0.032; plot (x<sc) ? x : sc+(1-sc)*((x-sc)**sf) / ((1-sc)**sf);\n \
               if(d > sc) \n						\
                 d = sc + (1.0 - sc) * pow(d - sc, sf) / pow(1.0 - sc, sf);\n \
               w = 1.0 - d;\n						\
               sumcoul += w * (vec4(1.0) - Ir_sign * (vec4(1.0) - pow(vec4(w), Ir_abs))) * coul; \
               sumn += w;\n						\
             }\n							\
           }\n								\
         }\n								\
         if(sumn >= 1.0 - sc)\n						\
           fragColor = sumcoul / sumn;\n				\
         else\n								\
           fragColor = vec4(0.0);\n					\
       }\n								\
     }";

static void init(void) {
  int i;
  if(!_pId[0]) {
    _pId[0] = gl4duCreateProgram(gl4dfBasicVS, gl4dfMCMD_mdFS, NULL);
    _pId[1] = gl4duCreateProgram(gl4dfBasicVS, gl4dfMCMD_select4mcmdFS, NULL);
    _pId[2] = gl4duCreateProgram(gl4dfBasicVS, gl4dfMCMD_mdbuV0FS, NULL);
    _pId[3] = gl4duCreateProgram(gl4dfBasicVS, gl4dfMCMD_mdbuV1FS, NULL);
  }
  if(!_mdTexId[0])
    glGenTextures(4, _mdTexId);
  if(!_tempTexId[0])
    glGenTextures((sizeof _tempTexId / sizeof *_tempTexId), _tempTexId);
  _maxLevel = mdTexData(_width, _height);
  for(i = 0; i < (sizeof _tempTexId / sizeof *_tempTexId); i++) {
    glBindTexture(GL_TEXTURE_2D, _tempTexId[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  gl4duAtExit(quit);
}

static void quit(void) {
  if(_mdTexId[0]) {
    glDeleteTextures(4, _mdTexId);
    _mdTexId[0] = 0;
  }
  if(!_tempTexId[0]) {
    glDeleteTextures((sizeof _tempTexId / sizeof *_tempTexId), _tempTexId);
    _tempTexId[0] = 0;
  }
  _pId[0] = 0;
}

#define UNDEFINED_PARENT ((GLushort)-1)
#define UNDEFINED_CHILD  ((GLushort)-1)
#define InMap(x, y, w, h) ( (x) >= 0 && (x) < (w) && (y) >= 0 && (y) < (h) )

#define putChildPos(cmap, i, v) do {\
    (cmap)[4 * (i) + 0] = ((v) >> 24) & 0xFF;	\
    (cmap)[4 * (i) + 1] = ((v) >> 16) & 0xFF;	\
    (cmap)[4 * (i) + 2] = ((v) >> 8)  & 0xFF;	\
    (cmap)[4 * (i) + 3] = ((v))       & 0xFF;	\
  } while(0)
#define getChildPos(cmap, i) ( (((unsigned long)((cmap)[4 * (i) + 0])) << 24) | \
			       (((unsigned long)((cmap)[4 * (i) + 1])) << 16) |	\
			       (((unsigned long)((cmap)[4 * (i) + 2])) <<  8) |	\
			       (((unsigned long)((cmap)[4 * (i) + 3])) <<  0) )

typedef struct ll_t ll_t;
struct ll_t {
  GLushort x, y;
  struct ll_t * next;
};

static inline int nbLevels(int w, int h) {
  return 2 * (int)ceil(log(MAX(w, h)) / log(2.0));
}

static inline void llAdd(ll_t ** here, GLushort x, GLushort y) {
  *here = malloc(sizeof ** here);
  assert(*here);
  (*here)->x = x;
  (*here)->y = y;
  (*here)->next = NULL;
}

static inline void llInsert(ll_t ** head, GLushort x, GLushort y) {
  while(*head)
    head = &((*head)->next);
  llAdd(head, x, y);
}

static inline void llFree(ll_t ** head) {
  ll_t * ptr = *head, * tmp;
  while(ptr) {
    tmp = ptr;
    ptr = ptr->next;
    free(tmp);
  }
  *head = NULL;
}

static inline ll_t ** llMapNew(GLushort mapWidth, GLushort mapHeight) {
  ll_t ** llmap = calloc(mapWidth * mapHeight, sizeof * llmap);
  assert(llmap);
  return llmap;
}

static inline void llMapFree(ll_t ** llmap, GLushort mapWidth, GLushort mapHeight) {
  GLuint i;
  for(i = 0; i < mapWidth * mapHeight; i++)
    llFree(&(llmap[i]));
  free(llmap);
}

static inline GLushort getParentDataX(GLubyte * parentData, GLushort mapWidth, int x, int y, int i) {
  int d = ((y * mapWidth + x) << 4) + (i << 2);
  return (((GLushort)parentData[d]) << 8) | ((GLushort)parentData[d + 1]);
}

static inline GLushort getParentDataY(GLubyte * parentData, GLushort mapWidth, int x, int y, int i) {
  int d = ((y * mapWidth + x) << 4) + (i << 2) + 2;
  return (((GLushort)parentData[d]) << 8) | ((GLushort)parentData[d + 1]);
}

static inline void setParentDataX(GLubyte * parentData, GLushort value, GLushort mapWidth, int x, int y, int i) {
  int d = ((y * mapWidth + x) << 4) + (i << 2);
  parentData[d]     = (GLubyte)(value >> 8);
  parentData[d + 1] = (GLubyte)(value & 0xFF);
}

static inline void setParentDataY(GLubyte * parentData, GLushort value, GLushort mapWidth, int x, int y, int i) {
  int d = ((y * mapWidth + x) << 4) + (i << 2) + 2;
  parentData[d]     = (GLubyte)(value >> 8);
  parentData[d + 1] = (GLubyte)(value & 0xFF);
}

static void diamondSquare(GLubyte * parentData, GLubyte * levelData, ll_t ** llmap, GLushort mapWidth, GLushort mapHeight, int x0, int y0, int w, int h, int current_level, int computing_level) {
  int x[14], y[14], i, w_2, w_21, h_2, h_21;
  w_2 = w >> 1; w_21 = w_2 + (w & 1); 
  h_2 = h >> 1; h_21 = h_2 + (h & 1);

  if(current_level < computing_level) { /* "!=" car jamais ">" */
    if((++current_level) & 1)
      diamondSquare(parentData, levelData, llmap, mapWidth, mapHeight, x0, y0,  w,  h, current_level, computing_level);
    else if(w_21 > 1 || h_21 > 1) {
      x[7] = x[0] = x0; x[9] = x[1] = x0 + w_2;
      y[1] = y[0] = y0; y[7] = y[9] = y0 + h_2;
      diamondSquare(parentData, levelData, llmap, mapWidth, mapHeight, x[0], y[0],  w_2,  h_2, current_level, computing_level);
      diamondSquare(parentData, levelData, llmap, mapWidth, mapHeight, x[1], y[1], w_21,  h_2, current_level, computing_level);
      diamondSquare(parentData, levelData, llmap, mapWidth, mapHeight, x[9], y[9], w_21, h_21, current_level, computing_level);
      diamondSquare(parentData, levelData, llmap, mapWidth, mapHeight, x[7], y[7],  w_2, h_21, current_level, computing_level);
    }
  } else if(current_level & 1) { /* Diamond */
    int d = 1, p = 2;
    x[6]  = x[7]  = x[8] = x[0] = x0;
    x[10] = x[12] = x[5] = x[9] = x[1] = x0 + w_2;
    x[3]  = x[4]  = x[2] = x0 + w;
    x[11] = x0 + w + w_21; x[13] = x0 - w_2;
    y[1]  = y[2]  = y[8] = y[0] = y0;
    y[11] = y[13] = y[7] = y[9] = y[3] = y0 + h_2;
    y[5]  = y[6]  = y[4] = y0 + h;
    y[10] = y0 - h_2; y[12] = y0 + h + h_21;
    if(w_2 && !h_2)
      p = 4;
    else if(!w_2 && h_2) {
      d = 3; p = 4;
    } else if(!w_2 && !h_2)
      return;
    for(i = d; i < 8; i += p) {
      if(getParentDataX(parentData, mapWidth, x[i], y[i], 0) == UNDEFINED_PARENT) {
	levelData[y[i] * mapWidth + x[i]] = current_level;
	setParentDataX(parentData, x[i - 1], mapWidth, x[i], y[i], 0);
	setParentDataY(parentData, y[i - 1], mapWidth, x[i], y[i], 0);
	setParentDataX(parentData, x[i + 1], mapWidth, x[i], y[i], 1);
	setParentDataY(parentData, y[i + 1], mapWidth, x[i], y[i], 1);
	setParentDataX(parentData,     x[9], mapWidth, x[i], y[i], 2);
	setParentDataY(parentData,     y[9], mapWidth, x[i], y[i], 2);
	if(InMap(x[i], y[i], mapWidth, mapHeight)) {
	  llInsert(&(llmap[y[i - 1] * mapWidth + x[i - 1]]), x[i], y[i]);
	  assert(x[i - 1] != x[i] || y[i - 1] != y[i]);
	  llInsert(&(llmap[y[i + 1] * mapWidth + x[i + 1]]), x[i], y[i]);
	  assert(x[i + 1] != x[i] || y[i + 1] != y[i]);
	  llInsert(&(llmap[y[9] * mapWidth + x[9]]),         x[i], y[i]);
	  assert(x[9] != x[i] || y[9] != y[i]);
	}
	if(x[i + 9 - ((i - 1) >> 1)] >= 0 && x[i + 9 - ((i - 1) >> 1)] < mapWidth && 
	   y[i + 9 - ((i - 1) >> 1)] >= 0 && y[i + 9 - ((i - 1) >> 1)] < mapHeight) {
	  setParentDataX(parentData, x[i + 9 - ((i - 1) >> 1)], mapWidth, x[i], y[i], 3);
	  setParentDataY(parentData, y[i + 9 - ((i - 1) >> 1)], mapWidth, x[i], y[i], 3);
	  if(InMap(x[i], y[i], mapWidth, mapHeight)) {
	    llInsert(&(llmap[y[i + 9 - ((i - 1) >> 1)] * mapWidth + x[i + 9 - ((i - 1) >> 1)]]), x[i], y[i]);
	    assert(x[i + 9 - ((i - 1) >> 1)] != x[i] || y[i + 9 - ((i - 1) >> 1)] != y[i]);
	  }
	}
      }
    }
  } else if(w_2 || h_2) { /* Square */
    x[6] = x[0] = x0; x[9] = x0 + w_2; x[4] = x[2] = x0 + w;
    y[2] = y[0] = y0; y[9] = y0 + h_2; y[6] = y[4] = y0 + h;
    if(getParentDataX(parentData, mapWidth, x[9], y[9], 0) == UNDEFINED_PARENT) {
      levelData[y[9] * mapWidth + x[9]] = current_level;
      for(i = 0; i < 4; i++) {
	setParentDataX(parentData, x[i << 1], mapWidth, x[9], y[9], i);
	setParentDataY(parentData, y[i << 1], mapWidth, x[9], y[9], i);
	if(InMap(x[i << 1], y[i << 1], mapWidth, mapHeight)) {
	  llInsert(&(llmap[y[i << 1] * mapWidth + x[i << 1]]), x[9], y[9]);
	  assert(x[i << 1] != x[9] || y[i << 1] != y[9]);
	}
      }
    }
  }
}

static void diamondSquare2Tex(GLubyte ** parentData, GLubyte ** levelData, GLushort ** childData, GLuint * childDataSize, GLubyte ** childPos, unsigned int w, unsigned int h) {
  int i, l, sl, maxsl = 0;
  ll_t ** llmap = llMapNew(w, h);
  *parentData = malloc(4 * w * 4 * h * sizeof ** parentData); assert(*parentData);
  *levelData = malloc(w * h * sizeof ** levelData); assert(*levelData);
  *childData = malloc((*childDataSize = 2) * sizeof ** childData);  assert(*childData);
  *childPos  = malloc(4 * w * h * sizeof ** childPos); assert(*childPos);
  
  memset(*parentData, 0xFF, 4 * w * 4 * h * sizeof ** parentData);
  memset(*levelData, 0xFF, w * h * sizeof ** levelData);
  for(i = 0; i < nbLevels(w, h); i++)
    diamondSquare(*parentData, *levelData, llmap, w, h, 0, 0,  w - 1,  h - 1, 0, i);
  
  for(i = 0, l = 0; i < w * h; i++) {
    ll_t * ptr;
    putChildPos(*childPos, i, l);
    for(ptr = llmap[i], sl = 0; ; ptr = ptr->next) {
      (*childData)[l++] = ptr ? ptr->x : UNDEFINED_CHILD;
      (*childData)[l++] = ptr ? ptr->y : UNDEFINED_CHILD;
      if(l == *childDataSize) {
	(*childData) = realloc(*childData, (*childDataSize <<= 1) * sizeof ** childData);
	assert(*childData);
      }
      if(!ptr)
	break;
      else if(maxsl < ++sl)
	maxsl = sl;
    }
  }
  /* fprintf(stderr, "max children = %d\n", maxsl); */
  llMapFree(llmap, w, h);
}

static int mdTexData(unsigned int w, unsigned int h) {
  int i, l = 0;
  GLushort * childData;
  GLubyte * childPos, * parentData = NULL, * levelData = NULL;
  
  diamondSquare2Tex(&parentData, &levelData, &childData, &_buTreeSize, &childPos, w, h);
  
  glBindTexture(GL_TEXTURE_2D, _mdTexId[3]);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, (int)sqrt(_buTreeSize), _buTreeSize/(int)sqrt(_buTreeSize), 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, childData);
  free(childData);
  
  glBindTexture(GL_TEXTURE_2D, _mdTexId[2]);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, childPos);
  free(childPos);
  
  glBindTexture(GL_TEXTURE_2D, _mdTexId[1]);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, levelData);
  for(i = l = 0; i < w * h; i++)
    if(levelData[i] != 255 && l < levelData[i])
      l = levelData[i];
  free(levelData);

  glBindTexture(GL_TEXTURE_2D, _mdTexId[0]);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4 * w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, parentData);
  free(parentData);
  return l;
}
