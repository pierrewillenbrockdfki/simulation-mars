/*
 *  Copyright 2016, DFKI GmbH Robotics Innovation Center
 *
 *  This file is part of the MARS simulation framework.
 *
 *  MARS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3
 *  of the License, or (at your option) any later version.
 *
 *  MARS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with MARS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 *  OsgMaterial.cpp
 *  General OsgMaterial to inherit from.
 *
 *  Created by Langosz on 2016
 */

#include <mars/utils/misc.h>
#include "OsgMaterial.h"
#include "OsgMaterialManager.h"
#include "MaterialNode.h"
#include <osgDB/WriteFile>

#include "shader/ShaderFactory.h"
#include "shader/DRockGraphSP.h"
#include "shader/YamlSP.h"
#include "shader/yaml-shader.h"
#include "shader/PhobosGraphSP.h"

#include <osg/TexMat>
#include <osg/CullFace>

#include <opencv2/opencv.hpp>

#include <cmath>

namespace osg_material_manager {

  using namespace std;
  using namespace mars::utils;
  using namespace configmaps;

  OsgMaterial::OsgMaterial(std::string resPath)
    : no_update(false), material(0),
      hasShaderSources(false), isInit(false),
      useShader(true),
      useShadow(false),
      maxNumLights(1),
      invShadowTextureSize(1./1024),
      useWorldTexCoords(false),
      resPath(resPath),
      loadPath(""),
      shadowTechnique("none") {
    noiseMapUniform = new osg::Uniform("NoiseMap", NOISE_MAP_UNIT);
    texScaleUniform = new osg::Uniform("texScale", 1.0f);
    sinUniform = new osg::Uniform("sinUniform", 0.0f);
    cosUniform = new osg::Uniform("cosUniform", 1.0f);
    shadowScaleUniform = new osg::Uniform("shadowScale", 0.5f);
    bumpNorFacUniform = new osg::Uniform("bumpNorFac", 1.0f);
    shadowSamples = 1;
    shadowSamplesUniform = new osg::Uniform("shadowSamples", 1);
    invShadowSamplesUniform = new osg::Uniform("invShadowSamples",
                                               1.f/1);
    invShadowTextureSizeUniform = new osg::Uniform("invShadowTextureSize",
                                                   (float)(invShadowTextureSize));

    envMapSpecularUniform = new osg::Uniform("envMapSpecular", osg::Vec4f(0.0f, 0.0f, 0.0f, 0.0f));
    envMapScaleUniform = new osg::Uniform("envMapScale", osg::Vec4f(0.0f, 0.0f, 0.0f, 0.0f));
    terrainScaleZUniform = new osg::Uniform("terrainScaleZ", 0.0f);
    terrainDimUniform = new osg::Uniform("terrainDim", 0);
    noiseMap = new osg::Texture2D();
    noiseMap->setDataVariance(osg::Object::DYNAMIC);
    noiseMap->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    noiseMap->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    noiseMap->setWrap(osg::Texture::WRAP_R, osg::Texture::REPEAT);
    noiseMap->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
    noiseMap->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);
    unitMap["diffuseMap"] = 0;
    unitMap["normalMap"] = 5;
    unitMap["displacementMap"] = 6;
    unitMap["environmentMap"] = 0;
    unitMap["envMapR"] = 8;
    unitMap["envMapG"] = 9;
    unitMap["envMapB"] = 10;
    unitMap["envMapA"] = 11;
    unitMap["normalMapR"] = 12;
    unitMap["normalMapG"] = 13;
    unitMap["normalMapB"] = 14;
    unitMap["terrainMap"] = 6;
    t = 0;
  }

  OsgMaterial::~OsgMaterial() {
  }

  osg::Vec4 OsgMaterial::getColor(string key) {
    osg::Vec4 c(0, 0, 0, 1);
    if(map.hasKey(key)) {
      ConfigMap &m = map[key];
      c[0] = m.get("r", 0.0);
      c[1] = m.get("g", 0.0);
      c[2] = m.get("b", 0.0);
      c[3] = m.get("a", 1.0);
    }
    return c;
  }
  // the material struct can also contain a static texture (texture file)
  void OsgMaterial::setMaterial(const ConfigMap &map_) {
    //return;
    map = map_;
    material = new osg::Material();

    if(map.hasKey("maxNumLights")) {
      maxNumLights = map["maxNumLights"];
      fprintf(stderr, "...+++ set maxNumLights: %d\n", maxNumLights);
    }

    // reinit if the material is already created
    if(isInit) initMaterial();
  }

  void OsgMaterial::initMaterial() {
    isInit = true;
    //map.toYamlFile("material.yml");
    if(map.hasKey("loadPath")) {
      loadPath << map["loadPath"];
      if(loadPath[loadPath.size()-1] != '/') {
        loadPath.append("/");
      }
    }
    if(map.hasKey("filePrefix")) {
      loadPath << map["filePrefix"];
      if(loadPath[loadPath.size()-1] != '/') {
        loadPath.append("/");
      }
    }
    name << map["name"];
    getLight = map.get("getLight", true);

    // create the osg::Material
    material->setColorMode(osg::Material::OFF);

    material->setAmbient(osg::Material::FRONT_AND_BACK, getColor("ambientColor"));
    material->setSpecular(osg::Material::FRONT_AND_BACK, getColor("specularColor"));
    material->setDiffuse(osg::Material::FRONT_AND_BACK, getColor("diffuseColor"));
    material->setEmission(osg::Material::FRONT_AND_BACK, getColor("emissionColor"));
    material->setShininess(osg::Material::FRONT_AND_BACK, map.get("shininess", 0.0));
    material->setTransparency(osg::Material::FRONT_AND_BACK, map.get("transparency", 0.0));

    // get the StateSet of the Object
    osg::StateSet *state = getOrCreateStateSet();

    // set the material
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);

    if (map.hasKey("culling") && !map["culling"]) {
      state->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
    }

    if(!getLight) {
      osg::ref_ptr<osg::CullFace> cull = new osg::CullFace();
      cull->setMode(osg::CullFace::BACK);
      state->setAttributeAndModes(cull.get(), osg::StateAttribute::OFF);
      state->setMode(GL_LIGHTING,
                     osg::StateAttribute::OFF);
      state->setMode(GL_FOG, osg::StateAttribute::OFF);
    }

    float transparency = (float)map.get("transparency", 0.0);
    string texturename = map.get("diffuseTexture", std::string());
    double tex_scale = map.get("tex_scale", 1.0);
    texScaleUniform->set((float)tex_scale);

    //disable all textures
    std::map<std::string, TextureInfo>::iterator it = textures.begin();
    for(; it!=textures.end(); ++it) {
      it->second.enabled = false;
      if(it->second.cubemap) {
        state->setTextureAttributeAndModes(it->second.unit,
                                           it->second.cubemap,
                                           osg::StateAttribute::OFF);
      } else {
        state->setTextureAttributeAndModes(it->second.unit,
                                           it->second.texture,
                                           osg::StateAttribute::OFF);
      }
      state->removeUniform(it->second.textureUniform);
    }
    if (!texturename.empty()) {
      ConfigMap config;
      config["name"] = "diffuseMap";
      config["file"] = texturename;
      config["texScale"] = tex_scale;
      addTexture(config, map.hasKey("instancing"));
    }

    bool generateTangents = false;
    texturename = map.get("normalTexture", std::string());
    if (!texturename.empty()) {
      generateTangents = true;
      ConfigMap config;
      config["name"] = "normalMap";
      config["file"] = texturename;
      config["texScale"] = tex_scale;
      addTexture(config, map.hasKey("instancing"));
    }
    bumpNorFacUniform->set((float)map.get("bumpNorFac", 1.0));

    if(map.hasKey("textures")) {
      ConfigVector::iterator it = map["textures"].begin();
      for(; it!=map["textures"].end(); ++it) {
        if(it->hasKey("layer")) {
          addTextureArray(*it);
        }
        else if(it->hasKey("cubemap")) {
          addCubemap(*it);
        }
        else {
          addTexture(*it, map.hasKey("instancing"));
        }
      }
    }
    useWorldTexCoords = map.get("useWorldTexCoords", false);

    updateShader(true);

    {
      std::vector<osg::ref_ptr<MaterialNode> >::iterator it = materialNodeVector.begin();
      for(; it!=materialNodeVector.end(); ++it) {
        if(generateTangents) {
          (*it)->setNeedTangents(true);
          //(*it)->generateTangents();
        }
        (*it)->setTransparency(transparency);
      }
    }
  }
    
  void OsgMaterial::addCubemap(ConfigMap &config) {
    osg::StateSet *state = getOrCreateStateSet();
    std::map<std::string, TextureInfo>::iterator it = textures.find((std::string)config["name"]);
    if(it != textures.end()) {
      TextureInfo &info = it->second;
      // todo: handle changes in texture unit etc.
      std::map<std::string, int> mapping;
      mapping["north"] = osg::TextureCubeMap::POSITIVE_Z;
      mapping["east"] = osg::TextureCubeMap::POSITIVE_X;
      mapping["south"] = osg::TextureCubeMap::NEGATIVE_Z;
      mapping["west"] = osg::TextureCubeMap::NEGATIVE_X;
      mapping["up"] = osg::TextureCubeMap::NEGATIVE_Y;
      mapping["down"] = osg::TextureCubeMap::POSITIVE_Y;

      for(auto it: mapping) {
        std::string file = config["cubemap"][it.first];
        if(!loadPath.empty() && file[0] != '/') {
          file = loadPath + file;
        }
        info.cubemap->setImage(it.second,
                               OsgMaterialManager::loadImage(file));
      }
      if(!info.enabled) {
        state->setTextureAttributeAndModes(info.unit, info.cubemap,
                                           osg::StateAttribute::ON);
        state->addUniform(info.textureUniform.get());
        info.enabled = true;
      }
    }
    else {
      TextureInfo info;
      info.name << config["name"];
      info.cubemap = OsgMaterialManager::loadCubemap(config["cubemap"],
                                                     loadPath);

      info.unit = 0;
      if(unitMap.hasKey(info.name)) {
        info.unit = unitMap[info.name];
      }
      if(config.hasKey("unit")) {
        info.unit = config["unit"];
      }
      fprintf(stderr, "set unit: %d\n", info.unit);
      info.textureUniform = new osg::Uniform(info.name.c_str(), info.unit);
      state->setTextureAttributeAndModes(info.unit, info.cubemap,
                                         osg::StateAttribute::ON);
      state->addUniform(info.textureUniform.get());
      info.enabled = true;
      textures[info.name] = info;
    }
  }

  void OsgMaterial::addTexture(ConfigMap &config, bool nearest) {
    osg::StateSet *state = getOrCreateStateSet();
    std::map<std::string, TextureInfo>::iterator it = textures.find((std::string)config["name"]);
    if(it != textures.end()) {
      TextureInfo &info = it->second;
      // todo: handle changes in texture unit etc.
      std::string file = config["file"];
      if(!loadPath.empty() && file[0] != '/') {
        file = loadPath + file;
      }
      info.texture = OsgMaterialManager::loadTexture(file);
      if(!info.enabled) {
        state->setTextureAttributeAndModes(info.unit, info.texture,
                                           osg::StateAttribute::ON);
        state->addUniform(info.textureUniform.get());
        info.enabled = true;
      }
    }
    else {
      TextureInfo info;
      info.name << config["name"];
      fprintf(stderr, "load texture: %s\n", info.name.c_str());
      std::string file = config["file"];
      if(!loadPath.empty() && file[0] != '/') {
        file = loadPath + file;
      }
      fprintf(stderr, "     texture file: %s\n", file.c_str());
      if(info.name == "terrainMap") {
        info.texture = loadTerrainTexture(file);
        nearest = true;
      }
      else {
        info.texture = OsgMaterialManager::loadTexture(file);
      }
      int filter = 1;
      if(nearest) {
        filter = 0;
      }
      if(config.hasKey("filter")) {
        filter = config["filter"];
      }
      if(filter == 0) {
        fprintf(stderr, "set filter to nearest: %s\n", info.name.c_str());
        info.texture->setFilter(osg::Texture::MIN_FILTER,
                                osg::Texture::NEAREST);
        info.texture->setFilter(osg::Texture::MAG_FILTER,
                                osg::Texture::NEAREST);
      }
      else if(filter == 1) {
        info.texture->setFilter(osg::Texture::MIN_FILTER,
                                osg::Texture::LINEAR_MIPMAP_LINEAR);
        info.texture->setFilter(osg::Texture::MAG_FILTER,
                                osg::Texture::LINEAR);
      }
      else if(filter == 2) {
        info.texture->setFilter(osg::Texture::MIN_FILTER,
                                osg::Texture::LINEAR);
        info.texture->setFilter(osg::Texture::MAG_FILTER,
                                osg::Texture::LINEAR);
      }
      info.texture->setWrap( osg::Texture::WRAP_S, osg::Texture::REPEAT );
      info.texture->setWrap( osg::Texture::WRAP_T, osg::Texture::REPEAT );
      info.texture->setMaxAnisotropy(8);
      info.unit = 0;
      if(unitMap.hasKey(info.name)) {
        info.unit = unitMap[info.name];
      }
      if(config.hasKey("unit")) {
        info.unit = config["unit"];
      }
      fprintf(stderr, "set unit: %d\n", info.unit);
      info.textureUniform = new osg::Uniform(info.name.c_str(), info.unit);
      state->setTextureAttributeAndModes(info.unit, info.texture,
                                         osg::StateAttribute::ON);
      state->addUniform(info.textureUniform.get());
      if(config.hasKey("texScale") && (double)config["texScale"] != 1.0) {
        osg::ref_ptr<osg::TexMat> scaleTexture = new osg::TexMat();
        float tex_scale = (double)config["texScale"];
        scaleTexture->setMatrix(osg::Matrix::scale(tex_scale, tex_scale,
                                                   tex_scale));
        state->setTextureAttributeAndModes(info.unit, scaleTexture.get(),
                                           osg::StateAttribute::ON);
      }
      info.enabled = true;
      textures[info.name] = info;
    }
  }

    void OsgMaterial::addTextureArray(ConfigMap &config, bool nearest) {
    osg::StateSet *state = getOrCreateStateSet();
    std::map<std::string, TextureArrayInfo>::iterator it = textureArrays.find((std::string)config["name"]);
    if(it != textureArrays.end()) {
      TextureArrayInfo &info = it->second;
      // todo: handle changes in texture unit etc.
      int layer = config["layer"];
      std::string file = config["file"];
      if(!loadPath.empty() && file[0] != '/') {
        file = loadPath + file;
      }
      osg::ref_ptr<osg::Image> image = OsgMaterialManager::loadImage(file);
      info.texture->setImage(layer, image.get());
      if(!info.enabled) {
        state->setTextureAttributeAndModes(info.unit, info.texture,
                                           osg::StateAttribute::ON);
        state->addUniform(info.textureUniform.get());
        info.enabled = true;
      }
    }
    else {
      TextureArrayInfo info;
      info.name << config["name"];
      fprintf(stderr, "generate texture array: %s\n", info.name.c_str());
      int layer = config["layer"];
      std::string file = config["file"];
      if(!loadPath.empty() && file[0] != '/') {
        file = loadPath + file;
      }
      fprintf(stderr, "     image file: %s\n", file.c_str());
      osg::ref_ptr<osg::Image> image = OsgMaterialManager::loadImage(file);
      info.texture->setImage(layer, image.get());
      int filter = 1;
      if(nearest) {
        filter = 0;
      }
      if(config.hasKey("filter")) {
        filter = config["filter"];
      }
      if(filter == 0) {
        fprintf(stderr, "set filter to nearest: %s\n", info.name.c_str());
        info.texture->setFilter(osg::Texture::MIN_FILTER,
                                osg::Texture::NEAREST);
        info.texture->setFilter(osg::Texture::MAG_FILTER,
                                osg::Texture::NEAREST);
      }
      else if(filter == 1) {
        info.texture->setFilter(osg::Texture::MIN_FILTER,
                                osg::Texture::LINEAR_MIPMAP_LINEAR);
        info.texture->setFilter(osg::Texture::MAG_FILTER,
                                osg::Texture::LINEAR);
      }
      else if(filter == 2) {
        info.texture->setFilter(osg::Texture::MIN_FILTER,
                                osg::Texture::LINEAR);
        info.texture->setFilter(osg::Texture::MAG_FILTER,
                                osg::Texture::LINEAR);
      }
      info.texture->setWrap( osg::Texture::WRAP_S, osg::Texture::REPEAT );
      info.texture->setWrap( osg::Texture::WRAP_T, osg::Texture::REPEAT );
      info.texture->setMaxAnisotropy(8);
      info.unit = 0;
      if(unitMap.hasKey(info.name)) {
        info.unit = unitMap[info.name];
      }
      if(config.hasKey("unit")) {
        info.unit = config["unit"];
      }
      fprintf(stderr, "set unit: %d\n", info.unit);
      info.textureUniform = new osg::Uniform(info.name.c_str(), info.unit);
      state->setTextureAttributeAndModes(info.unit, info.texture,
                                         osg::StateAttribute::ON);
      state->addUniform(info.textureUniform.get());
      if(config.hasKey("texScale") && (double)config["texScale"] != 1.0) {
        osg::ref_ptr<osg::TexMat> scaleTexture = new osg::TexMat();
        float tex_scale = (double)config["texScale"];
        scaleTexture->setMatrix(osg::Matrix::scale(tex_scale, tex_scale,
                                                   tex_scale));
        state->setTextureAttributeAndModes(info.unit, scaleTexture.get(),
                                           osg::StateAttribute::ON);
      }
      info.enabled = true;
      textureArrays[info.name] = info;
    }
  }

  void OsgMaterial::disableTexture(std::string name) {
    {
      std::map<std::string, TextureInfo>::iterator it = textures.find(name);
      if(it != textures.end()) {
        osg::StateSet *state = getOrCreateStateSet();
        it->second.enabled = false;
        if(it->second.cubemap) {
          state->setTextureAttributeAndModes(it->second.unit,
                                             it->second.cubemap,
                                             osg::StateAttribute::OFF);
        }
        else {
          state->setTextureAttributeAndModes(it->second.unit,
                                             it->second.texture,
                                             osg::StateAttribute::OFF);
        }
        state->removeUniform(it->second.textureUniform);
        return;
      }
    }
    {
      std::map<std::string, TextureArrayInfo>::iterator it = textureArrays.find(name);
      if(it != textureArrays.end()) {
        osg::StateSet *state = getOrCreateStateSet();
        it->second.enabled = false;
        state->setTextureAttributeAndModes(it->second.unit,
                                           it->second.texture,
                                           osg::StateAttribute::OFF);
        state->removeUniform(it->second.textureUniform);
      }
    }
  }

  void OsgMaterial::enableTexture(std::string name) {
    {
      std::map<std::string, TextureInfo>::iterator it = textures.find(name);
      if(it != textures.end()) {
        osg::StateSet *state = getOrCreateStateSet();
        it->second.enabled = true;
        if(it->second.cubemap) {
          state->setTextureAttributeAndModes(it->second.unit,
                                             it->second.cubemap,
                                             osg::StateAttribute::ON);
        } else {
          state->setTextureAttributeAndModes(it->second.unit,
                                             it->second.texture,
                                             osg::StateAttribute::ON);
        }
        state->addUniform(it->second.textureUniform);
        return;
      }
    }
    {
      std::map<std::string, TextureArrayInfo>::iterator it = textureArrays.find(name);
      if(it != textureArrays.end()) {
        osg::StateSet *state = getOrCreateStateSet();
        it->second.enabled = true;
        state->setTextureAttributeAndModes(it->second.unit,
                                           it->second.texture,
                                           osg::StateAttribute::ON);
        state->addUniform(it->second.textureUniform);
        return;
      }
    }
  }

  bool OsgMaterial::checkTexture(std::string name) {
    {
      std::map<std::string, TextureInfo>::iterator it = textures.find(name);
      if(it != textures.end()) {
        return it->second.enabled;
      }
    }
    {
      std::map<std::string, TextureArrayInfo>::iterator it = textureArrays.find(name);
      if(it != textureArrays.end()) {
        return it->second.enabled;
      }
    }
    return false;
  }

  osg::ref_ptr<osg::Texture2D> OsgMaterial::getTexture(const std::string &name) {
    std::map<std::string, TextureInfo>::iterator it = textures.find(name);
    if(it != textures.end()) {
      return it->second.texture;
    }
    return NULL;
  }

  void OsgMaterial::setColor(string color, string key, string value) {
    double v = atof(value.c_str());
    if(key[key.size()-1] == 'a') map[color]["a"] = v;
    else if(key[key.size()-1] == 'r') map[color]["r"] = v;
    else if(key[key.size()-1] == 'g') map[color]["g"] = v;
    else if(key[key.size()-1] == 'b') map[color]["b"] = v;
    setMaterial(map);
  }

  void OsgMaterial::edit(const std::string &key, const std::string &value) {
    if(matchPattern("*/ambientColor/*", key) ||
       matchPattern("*/ambientFront/*", key)) {
      setColor("ambientColor", key, value);
    }
    else if(matchPattern("*/diffuseColor/*", key) ||
            matchPattern("*/diffuseFront/*", key)) {
      setColor("diffuseColor", key, value);
    }
    if(matchPattern("*/specularColor/*", key) ||
       matchPattern("*/specularFront/*", key)) {
      setColor("specularColor", key, value);
    }
    if(matchPattern("*/emissionColor/*", key) ||
       matchPattern("*/emissionFront/*", key)) {
      setColor("emissionColor", key, value);
    }

    if(matchPattern("*/diffuseTexture", key) ||
       matchPattern("*/texturename", key)) {
      if(value == "") {
        map["diffuseTexture"] = string();
        fprintf(stderr, "edit material: %s\n", map["diffuseTexture"].c_str());
      }
      else if(pathExists(value)) {
        map["diffuseTexture"] = mars::utils::trim(value);
        fprintf(stderr, "edit material: %s\n", map["diffuseTexture"].c_str());
      }
      setMaterial(map);
    }
    if(matchPattern("*/normalTexture", key) ||
       matchPattern("*/bumpmap", key)) {
      if(value == "") {
        map["normalTexture"] = string();
        fprintf(stderr, "edit material: %s\n", map["normalTexture"].c_str());
      }
      else if(pathExists(value)) {
        map["normalTexture"] = mars::utils::trim(value);
        fprintf(stderr, "edit material: %s\n", map["normalTexture"].c_str());
      }
      setMaterial(map);
    }
    if(matchPattern("*/displacementTexture", key) ||
       matchPattern("*/displacementmap", key)) {
      if(value == "") {
        map["displacementTexture"] = string();
      }
      else if(pathExists(value)) {
        map["displacementTexture"] = mars::utils::trim(value);
      }
      setMaterial(map);
    }
    if(matchPattern("*/bumpNorFac", key)) {
      map["bumpNorFac"] = atof(value.c_str());
      setMaterial(map);
    }
    if(matchPattern("*/shininess", key)) {
      map["shininess"] = atof(value.c_str());
      setMaterial(map);
    }
    if(matchPattern("*/transparency", key)) {
      map["transparency"] = atof(value.c_str());
      setMaterial(map);
    }
    if(matchPattern("*/tex_scale", key)) {
      map["tex_scale"] = atof(value.c_str());
      setMaterial(map);
    }
    if(matchPattern("*/getLight", key)) {
      bool b = atoi(value.c_str());
      map["getLight"] = b;
      setMaterial(map);
    }
  }

  void OsgMaterial::setTexture(osg::Texture2D *texture) {
    if(textures.find("diffuseMap") != textures.end()) {
      TextureInfo &info = textures["diffuseMap"];
      info.texture = texture;
      if(!info.enabled) {
        osg::StateSet *state = getOrCreateStateSet();
        state->setTextureAttributeAndModes(info.unit, info.texture,
                                           osg::StateAttribute::ON);
      }
    }
  }

  void OsgMaterial::setBumpMap(const std::string &filename) {
    fprintf(stderr, "OsgMaterial: setBumpMap is deprecated use addTexture instead");
  }

  void OsgMaterial::setNormalMap(const std::string &filename) {
    fprintf(stderr, "OsgMaterial: setNormalMap is deprecated use addTexture instead");
  }

  void OsgMaterial::setUseShader(bool val) {
    //fprintf(stderr, "use shader: %d %d\n", useShader, val);
    if(useShader != val) {
      useShader = val;
      updateShader(true);
    }
  }

  void OsgMaterial::setUseShadow(bool val) {
    //fprintf(stderr, "use shader: %d %d\n", useShader, val);
    if(useShadow != val) {
      useShadow = val;
      updateShader(true);
    }
  }

  void OsgMaterial::setShadowTechnique(std::string val) {
    //fprintf(stderr, "use shader: %d %d\n", useShader, val);
    if(shadowTechnique != val) {
      shadowTechnique = val;
      updateShader(true);
    }
  }

  void OsgMaterial::setShadowScale(float v) {
    shadowScaleUniform->set(1.f/(v*v));
  }

  void OsgMaterial::updateShader(bool reload) {
    if(no_update) {
      return;
    }
    osg::StateSet* stateSet = getOrCreateStateSet();

    //return;
    if(!useShader || !getLight) {
      if(lastProgram.valid()) {
        stateSet->removeAttribute(lastProgram.get());
        lastProgram = NULL;
      }
      disableTexture("normalMap");
      stateSet->setTextureAttributeAndModes(NOISE_MAP_UNIT, noiseMap,
                                            osg::StateAttribute::OFF);
      return;
    }
    //enableTexture("normalMap");

    if(!reload && hasShaderSources) {
      // no need to regenerate, shader source did not changed
      return;
    }
    hasShaderSources = true;
    bool has_texture = (textures.size()>0);//checkTexture("environmentMap") || checkTexture("diffuseMap") || checkTexture("normalMap");
    stateSet->setTextureAttributeAndModes(NOISE_MAP_UNIT, noiseMap,
                                          osg::StateAttribute::ON);
    stateSet->removeUniform(envMapSpecularUniform.get());
    stateSet->removeUniform(envMapScaleUniform.get());
    stateSet->removeUniform(terrainScaleZUniform.get());
    stateSet->removeUniform(terrainDimUniform.get());
    ShaderFactory factory;
    osg::Program *glslProgram;

    if(!map.hasKey("shader")) {
      map["shader"]["PixelLightVertex"] = true;
      map["shader"]["PixelLightFragment"] = true;
      if(checkTexture("normalMap")) {
        map["shader"]["NormalMapVertex"] = true;
        map["shader"]["NormalMapFragment"] = true;
      }
    }

    if (map["shader"].hasKey("provider")) {
      if ((string)map["shader"]["provider"] == "DRockGraph") {
        ConfigMap options;
        options["numLights"] = maxNumLights;
        options["shadowSamples"] = shadowSamples;
        string vertexPath = map["shader"]["vertex"];
        string fragmentPath = map["shader"]["fragment"];
        if(!loadPath.empty() && vertexPath[0] != '/') {
          vertexPath = loadPath + vertexPath;
        }
        if(!loadPath.empty() && fragmentPath[0] != '/') {
          fragmentPath = loadPath + fragmentPath;
        }
        ConfigMap vertexModel = ConfigMap::fromYamlFile(vertexPath);
        ConfigMap fragmentModel = ConfigMap::fromYamlFile(fragmentPath);
        string shadowTechnique_ = "none";
        if(useShadow) {
          shadowTechnique_ = shadowTechnique;
        }

        DRockGraphSP *vertexProvider = new DRockGraphSP(resPath, vertexModel, options, shadowTechnique_);
        DRockGraphSP *fragmentProvider = new DRockGraphSP(resPath, fragmentModel, options, shadowTechnique_);
        factory.setShaderProvider(vertexProvider, SHADER_TYPE_VERTEX);
        factory.setShaderProvider(fragmentProvider, SHADER_TYPE_FRAGMENT);
        if(textures.find("terrainMap") != textures.end()) {
          stateSet->addUniform(terrainScaleZUniform.get());
          stateSet->addUniform(terrainDimUniform.get());
          terrainScaleZUniform->set((float)(double)map["scaleZ"]);
        }
      } else if ((string)map["shader"]["provider"] == "PhobosGraph") {
        ConfigMap options;
        options["numLights"] = maxNumLights;
        options["loadPath"] = loadPath;
        options["customPath"] = "";
        if (map["shader"].hasKey("custom")) {
          options["customPath"] = (string)map["shader"]["custom"];
        }
        string vertexPath = map["shader"]["vertex"];
        string fragmentPath = map["shader"]["fragment"];
        if(!loadPath.empty() && vertexPath[0] != '/') {
          vertexPath = loadPath + vertexPath;
        }
        if(!loadPath.empty() && fragmentPath[0] != '/') {
          fragmentPath = loadPath + fragmentPath;
        }
        ConfigMap vertexModel = ConfigMap::fromYamlFile(vertexPath);
        ConfigMap fragmentModel = ConfigMap::fromYamlFile(fragmentPath);
        PhobosGraphSP *vertexProvider = new PhobosGraphSP(resPath, vertexModel, options);
        PhobosGraphSP *fragmentProvider = new PhobosGraphSP(resPath, fragmentModel, options);
        factory.setShaderProvider(vertexProvider, SHADER_TYPE_VERTEX);
        factory.setShaderProvider(fragmentProvider, SHADER_TYPE_FRAGMENT);
      }
    } else {
      vector<string> args;
      stringstream s;
      s << maxNumLights;
      YamlSP *vertexShader = new YamlSP(resPath);
      YamlSP *fragmentShader = new YamlSP(resPath);
      if(map["shader"].hasKey("TerrainMapVertex")) {
        ConfigMap map2 = ConfigMap::fromYamlFile(resPath+"/shader/terrainMap_vert.yml");
        YamlShader *terrainMapVert = new YamlShader((string)map2["name"], args, map2, resPath);
        vertexShader->addShaderFunction(terrainMapVert);
        stateSet->addUniform(terrainScaleZUniform.get());
        stateSet->addUniform(terrainDimUniform.get());
        terrainScaleZUniform->set((float)(double)map["scaleZ"]);
      }
      if (map["shader"].hasKey("PixelLightVertex")) {
        ConfigMap map2 = ConfigMap::fromYamlFile(resPath+"/shader/plight_vert.yaml");
        map2["mappings"]["numLights"] = s.str();
        YamlShader *plightVert = new YamlShader((string)map2["name"], args, map2, resPath);
        vertexShader->addShaderFunction(plightVert);
      }
      if(map["shader"].hasKey("NormalMapVertex")) {
        ConfigMap map2 = ConfigMap::fromYamlFile(resPath+"/shader/bumpmapping_vert.yaml");
        YamlShader *bumpVert = new YamlShader((string)map2["name"], args, map2, resPath);
        vertexShader->addShaderFunction(bumpVert);
      }
      if(map["shader"].hasKey("EnvMapVertex")) {
        ConfigMap map2 = ConfigMap::fromYamlFile(resPath+"/shader/envMap_vert.yml");
        YamlShader *shader = new YamlShader((string)map2["name"], args, map2, resPath);
        vertexShader->addShaderFunction(shader);

      }
      if (map["shader"].hasKey("PixelLightFragment")) {
        ConfigMap map2 = ConfigMap::fromYamlFile(resPath+"/shader/plight_frag.yaml");
        map2["mappings"]["numLights"] = s.str();
        map2["mappings"]["shadowSamples"] = shadowSamples;
        YamlShader *plightFrag = new YamlShader((string)map2["name"], args, map2, resPath);
        if(checkTexture("diffuseMap")) {
          plightFrag->addMainVar( (GLSLVariable) { "vec4", "col", "texture2D(diffuseMap, texCoord)" }, 1);
          plightFrag->addUniform((GLSLUniform) {"sampler2D", "diffuseMap"});
        }
        fragmentShader->addShaderFunction(plightFrag);
        if(useShadow and shadowTechnique != "none") {
          ConfigMap map3 = ConfigMap::fromYamlFile(resPath+"/shader/shadow_"+shadowTechnique+".yaml");
          map3["mappings"]["shadowSamples"] = shadowSamples;
          YamlShader *shadowFrag = new YamlShader((string)map3["name"], args, map3, resPath);
          fragmentShader->addShaderFunction(shadowFrag);
        }
      }
      if(map["shader"].hasKey("NormalMapFragment")) {
        ConfigMap map2 = ConfigMap::fromYamlFile(resPath+"/shader/bumpmapping_frag.yaml");
        YamlShader *bumpFrag = new YamlShader((string)map2["name"], args, map2, resPath);
        fragmentShader->addShaderFunction(bumpFrag);
      }
      if(map["shader"].hasKey("EnvMapFragment")) {
        ConfigMap map2 = ConfigMap::fromYamlFile(resPath+"/shader/envMap_frag.yml");
        YamlShader *frag = new YamlShader((string)map2["name"], args, map2, resPath);
        fragmentShader->addShaderFunction(frag);

      }

      vertexShader->setupShaderEnv(SHADER_TYPE_VERTEX, map, has_texture, useWorldTexCoords);
      factory.setShaderProvider(vertexShader, SHADER_TYPE_VERTEX);
      fragmentShader->setupShaderEnv(SHADER_TYPE_FRAGMENT, map, has_texture, useWorldTexCoords);
      factory.setShaderProvider(fragmentShader, SHADER_TYPE_FRAGMENT);
    }
    stateSet->addUniform(noiseMapUniform.get());
    if(has_texture) {
      stateSet->addUniform(texScaleUniform.get());
      stateSet->addUniform(sinUniform.get());
      stateSet->addUniform(cosUniform.get());
    }
    else {
      stateSet->removeUniform(texScaleUniform.get());
    }
    if (map.hasKey("envMapSpecular")) {
      envMapSpecularUniform->set(osg::Vec4((double)map["envMapSpecular"]["r"],
                                           (double)map["envMapSpecular"]["g"],
                                           (double)map["envMapSpecular"]["b"],
                                           (double)map["envMapSpecular"]["a"]));
      stateSet->addUniform(envMapSpecularUniform.get());
    }
    if (map.hasKey("envMapScale")) {
      envMapScaleUniform->set(osg::Vec4((double)map["envMapScale"]["r"],
                                        (double)map["envMapScale"]["g"],
                                        (double)map["envMapScale"]["b"],
                                        (double)map["envMapScale"]["a"]));
      stateSet->addUniform(envMapScaleUniform.get());
    }
    if(map.hasKey("shaderSources")) {
      // load shader from text file
      // todo: handle uniforms in a way that we dont need to create the shader
      //       sources above
      glslProgram = new osg::Program();
      { // load vertex shader
        string file = map["shaderSources"]["vertexShader"];
        if(!loadPath.empty() && file[0] != '/') {
          file = loadPath + file;
        }
        std::ifstream t(file.c_str());
        std::stringstream buffer;
        buffer << t.rdbuf();
        string source = buffer.str();
        osg::Shader *shader = new osg::Shader(osg::Shader::VERTEX);
        glslProgram->addShader(shader);
        shader->setShaderSource( source );
      }
      { // load fragment shader
        string file = map["shaderSources"]["fragmentShader"];
        if(!loadPath.empty() && file[0] != '/') {
          file = loadPath + file;
        }
        std::ifstream t(file.c_str());
        std::stringstream buffer;
        buffer << t.rdbuf();
        string source = buffer.str();
        osg::Shader *shader = new osg::Shader(osg::Shader::FRAGMENT);
        glslProgram->addShader(shader);
        shader->setShaderSource( source );
      }
    } else {
      glslProgram = factory.generateProgram();
      if(map.hasKey("printShader") && (bool)map["printShader"]) {
        std::string source = factory.generateShaderSource(SHADER_TYPE_VERTEX);
        std::string filename = "shader_sources/" + name + "_vert.c";
        createDirectory("shader_sources");
        FILE *f = fopen(filename.c_str(), "w");
        fprintf(f, "%s", source.c_str());
        fclose(f);
        source = factory.generateShaderSource(SHADER_TYPE_FRAGMENT);
        filename = "shader_sources/" + name + "_frag.c";
        f = fopen(filename.c_str(), "w");
        fprintf(f, "%s", source.c_str());
        fclose(f);
      }
    }
    if(checkTexture("normalMap") || checkTexture("environmentMap")) {
      glslProgram->addBindAttribLocation( "vertexTangent", TANGENT_UNIT );
      stateSet->addUniform(bumpNorFacUniform.get());
    } else {
      stateSet->removeUniform(bumpNorFacUniform.get());
    }
    if(lastProgram.valid()) {
      stateSet->removeAttribute(lastProgram.get());
    }
    stateSet->setAttributeAndModes(glslProgram,
                                   osg::StateAttribute::ON);

    stateSet->removeUniform(shadowSamplesUniform.get());
    stateSet->removeUniform(invShadowSamplesUniform.get());
    stateSet->removeUniform(invShadowTextureSizeUniform.get());
    stateSet->removeUniform(shadowScaleUniform.get());

    stateSet->addUniform(shadowSamplesUniform.get());
    stateSet->addUniform(invShadowSamplesUniform.get());
    stateSet->addUniform(invShadowTextureSizeUniform.get());
    stateSet->addUniform(shadowScaleUniform.get());

    lastProgram = glslProgram;
  }

  void OsgMaterial::setNoiseImage(osg::Image *i) {
    noiseMap->setImage(i);
  }

  void OsgMaterial::update() {
    t += 0.04;
    if(t > 6.28) t -= 6.28;
    sinUniform->set((float)(sin(t)*0.5));
    cosUniform->set((float)(cos(t)*0.75));
  }

  void OsgMaterial::setShadowSamples(int v) {
    bool needUpdate = (shadowSamples != v);
    shadowSamples = v;
    shadowSamplesUniform->set(v);
    invShadowSamplesUniform->set(1.f/(v*v));
    if(needUpdate) updateShader(true);
  }

  void OsgMaterial::removeMaterialNode(MaterialNode* d) {
    std::vector<osg::ref_ptr<MaterialNode> >::iterator it;
    for(it=materialNodeVector.begin(); it!=materialNodeVector.end(); ++it) {
      if(it->get() == d) {
        materialNodeVector.erase(it);
        return;
      }
    }
  }

  void OsgMaterial::setShadowTextureSize(int size) {
    invShadowTextureSize = 1./size;
    invShadowTextureSizeUniform->set((float)invShadowTextureSize);
  }

  void OsgMaterial::addMaterialNode(MaterialNode *d) {
    if(!isInit) initMaterial();    

    materialNodeVector.push_back(d);
    if(checkTexture("normalMap") || checkTexture("displacementMap") || checkTexture("environmentMap")) {
      d->setNeedTangents(true);
    }
    if (map.get("instancing", false)) {
      double w, h, l;
      w = h = l = 1.0;
      if(map.hasKey("instancesWidth")) {
        w = map["instancesWidth"];
      }
      if(map.hasKey("instancesHeight")) {
        h = map["instancesHeight"];
      }
      if(map.hasKey("instancesLength")) {
        l = map["instancesLength"];
      }
      d->setNeedInstancing(true, map.get("numInstances", 1), w, h, l);
    }
    // renderbin have to be set before setting transparency
    // since it is applied in that function
    if(map.hasKey("renderBin")) {
      d->setRenderBin(map["renderBin"]);
    }
    d->setTransparency((float)map.get("transparency", 0.0));
  }

  void OsgMaterial::setMaxNumLights(int n) {
    if(map.hasKey("maxNumLights")) {
      return;
    }
    bool needUpdate = (maxNumLights != n);
    maxNumLights = n;
    if(needUpdate) updateShader(true);
  }

  int OsgMaterial::getMaxNumLights() {
    return maxNumLights;
  }

  osg::Texture2D* OsgMaterial::loadTerrainTexture(std::string filename) {
    cv::Mat img=cv::imread(filename, cv::IMREAD_ANYDEPTH);
    osg::Texture2D *texture = NULL;
    if(img.data) {
      texture = new osg::Texture2D;
      texture->setDataVariance(osg::Object::DYNAMIC);
      texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP);
      texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP);
      texture->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP);
      osg::Image* image = new osg::Image();
      image->allocateImage(img.cols, img.rows,
                           1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV);
      assert(img.cols == img.rows);
      if(img.cols != img.rows or img.channels() != 1) {
        fprintf(stderr, "ERROR: bad heightmap loaded: w=%d h=%d c=%d\n",
                img.cols, img.rows, img.channels());
        return texture;
      }

      terrainDimUniform->set((int)img.cols);
      cv::Scalar s;
      int depth = img.depth();
      int v;
      //double imageMaxValue = pow(2., depth);
      double s256 = 1./256;
      for(int x=0; x<img.cols; ++x) {
        for(int y=0; y<img.rows; ++y) {
          //s=cvGet2D(img,y,x);
          if(depth == CV_16U) {
            s = img.at<ushort>(y,x);
            v = floor(s[0]*s256);
            if(v < 0) v = 0;
            if(v>255) v = 255;
            image->data(y, x)[0] = (char)v;
            v = (int)s[0] % 256;
            if(v < 0) v = 0;
            if(v>255) v = 255;
            image->data(y, x)[1] = (char)v;
          }
          else {
            s = img.at<uchar>(y,x);
            image->data(y, x)[0] = (char)s.val[0];
            image->data(y, x)[1] = 0;
          }
          image->data(y, x)[2] = 0;
          image->data(y, x)[3] = 255;
        }
      }
      //osgDB::writeImageFile(*image, "da.png");

      texture->setImage(image);
      img.release();//cvReleaseImage(&img);
    }
    return texture;
  }

  int OsgMaterial::getNumInstances() {
    return map.get("numInstances", 1);
  }

  double OsgMaterial::getInstancesWidth() {
    return map.get("instancesWidth", 200.0);
  }

} // end of namespace osg_material_manager
