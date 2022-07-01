/*
 *  Copyright 2012, 2014, DFKI GmbH Robotics Innovation Center
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

/**
 * \file SMURFLoader.h
 * \author Malte Langosz, Kai von Szadkowski
 */
#ifndef SMURF_LOADER_H
#define SMURF_LOADER_H

#ifdef _PRINT_HEADER_
  #warning "SMURFLoader.h"
#endif

#include <mars/interfaces/sim/ControlCenter.h>
#include <mars/interfaces/sim/LoadCenter.h>
#include <mars/interfaces/sim/LoadSceneInterface.h>
#include <mars/entity_generation/entity_factory/EntityFactoryManager.h>
#include <configmaps/ConfigData.h>

namespace mars {
  namespace smurf {

    struct ConfigStruct {
      utils::Vector pos;
      utils::Quaternion rot;
      std::string anchor;
      std::string parent;

      ConfigStruct() : pos(0,0,0), rot(1,0,0,0), anchor("none"), parent("world") {}
    };

    class SMURFLoader : public interfaces::LoadSceneInterface {

    public:
      SMURFLoader(lib_manager::LibManager *theManager);
      ~SMURFLoader();

      // LibInterface methods
      int getLibVersion() const {return 1;}
      const std::string getLibName() const {return std::string("mars_smurf_loader");}
      CREATE_MODULE_INFO();

      virtual bool loadFile(std::string filename, std::string tmpPath,
                            std::string robotname);
      /* Overloading this enables us to use this method without changing
       * all the interfaces.
       * In the future it might be a good idea to implement passing of the
       * cfgstruct to other loaders, too, to apply the transformation there
       * as well.
      */
      virtual bool loadFile(std::string filename, std::string tmpPath,
                            std::string robotname, void* args,
                            bool do_not_create=false);
      virtual bool loadFile(std::string filename, std::string tmpPath,
                            std::string robotname, utils::Vector pos, utils::Vector rot);

      virtual int saveFile(std::string filename, std::string tmpPath);
      //virtual void loadEntity(configmaps::ConfigMap* it, std::string path);
      virtual void loadEntity(configmaps::ConfigMap* it, std::string path,
                              bool do_not_create=false);

      // SMURF-Loader specific functions
      void checkEncodings();
      unsigned int parseSVG(std::vector<configmaps::ConfigMap> *configList,
          std::string sceneFilename);

    private:
      std::string tmpPath;
      double global_width;
      double global_length;
      //std::map<std::string, std::string> params;
      //std::vector<configmaps::ConfigMap> entityList;
      //configmaps::ConfigMap courseconfig;

      interfaces::ControlCenter *control;
      entity_generation::EntityFactoryManager* factoryManager;
      std::vector<configmaps::ConfigMap> entitylist; // a list of the entities to be loaded

      unsigned int unzip(const std::string& destinationDir,
                         const std::string& zipFilename);
      void applyConfigStruct(ConfigStruct* cfg_struct, configmaps::ConfigMap& m,
        bool is_smurfa=false);
      void transformConfigMapPose(utils::Vector pos_offset,
        utils::Quaternion rot_offset, configmaps::ConfigMap* map);
      void getPoseFromConfigMap(configmaps::ConfigMap &map,
        utils::Vector* pos, utils::Quaternion* rot);
    };

  } // end of namespace smurf
} // end of namespace mars

#endif  // SMURF_LOADER_H
