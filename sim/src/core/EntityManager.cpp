/*
 *  Copyright 2011, 2012, DFKI GmbH Robotics Innovation Center
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

#include "EntityManager.h"
#include "SimEntity.h"
#include <configmaps/ConfigData.h>
#include <mars/interfaces/graphics/GraphicsManagerInterface.h>
#include <mars/interfaces/sim/EntitySubscriberInterface.h>
#include <mars/utils/MutexLocker.h>
#include <mars/utils/misc.h>

#include <iostream>
#include <string>

namespace mars {
  namespace sim {

    using namespace utils;
    using namespace interfaces;

    EntityManager::EntityManager(ControlCenter* c) {

      control = c;
      next_entity_id = 1;
      if (control->graphics)
        control->graphics->addEventClient((GraphicsEventClient*) this);
    }

    unsigned long EntityManager::addEntity(const std::string &name) {
      unsigned long id = 0;
      MutexLocker locker(&iMutex);
      entities[id = getNextId()] = new SimEntity(control, name);
      notifySubscribers(entities[id]);
      return id;
    }

    unsigned long EntityManager::addEntity(SimEntity* entity) {
      unsigned long id = 0;
      MutexLocker locker(&iMutex);
      entities[id = getNextId()] = entity;
      notifySubscribers(entity);
      return id;
    }

    void EntityManager::removeEntity(const std::string &name, bool completeAssembly) {
      SimEntity* entity = getEntity(name);
      if (completeAssembly && entity->getAssembly() != "") {
        removeAssembly(entity->getAssembly());
      } else {
        //remove from entity map
        for (auto it = entities.begin(); it != entities.end(); ++it) {
          if (it->second == entity) {
            entities.erase(it);
            break;
          }
        }
        //delete entity
        entity->removeEntity();
        /*TODO we have to free the memory here, but we don't know if this entity
        is allocated on the heap or the stack*/
      }
    }

    void EntityManager::appendConfig(const std::string &name, configmaps::ConfigMap &map) {
      SimEntity *entity = 0;
      //iterate over all robots to find the robot with the given name
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == name) {
          entity = iter->second;
          break;
        }
      }
      if (entity) {
        MutexLocker locker(&iMutex);
        entity->appendConfig(map);
      }
    }

    void EntityManager::removeAssembly(const std::string &assembly_name) {
      std::vector<SimEntity*> parts = getEntitiesOfAssembly(assembly_name);
      for (auto p: parts) {
        //remove from entity map
        for (auto it = entities.begin(); it != entities.end(); ++it) {
          if (it->second == p) {
            fprintf(stderr, "Deleting entity %s\n", p->getName().c_str());
            entities.erase(it);
            break;
          }
        }
        //delete entity
        p->removeEntity();
        // REVIEW Do we have to delete the anchor joints here?
      }
    }

    void EntityManager::notifySubscribers(SimEntity* entity) {
      for (std::vector<interfaces::EntitySubscriberInterface*>::iterator it = subscribers.begin();
           it != subscribers.end(); ++it) {
             (*it)->registerEntity(entity);
           }
    }

    const std::map<unsigned long, SimEntity*>* EntityManager::subscribeToEntityCreation(interfaces::EntitySubscriberInterface* newsub) {
      if (newsub!=nullptr) {
        subscribers.push_back(newsub);
      }
      return &entities;
    }

    void EntityManager::addNode(const std::string& entityName, long unsigned int nodeId,
        const std::string& nodeName) {
      SimEntity *entity = 0;
      //iterate over all robots to find the robot with the given name
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == entityName) {
          entity = iter->second;
          break;
        }
      }
      if (entity) {
        MutexLocker locker(&iMutex);
        entity->addNode(nodeId, nodeName);
      }
    }

    void EntityManager::addMotor(const std::string& entityName, long unsigned int motorId,
        const std::string& motorName) {
      //iterate over all robots to find the robot with the given name
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == entityName) {
          MutexLocker locker(&iMutex);
          iter->second->addMotor(motorId, motorName);
          break;
        }
      }
    }

    void EntityManager::addJoint(const std::string& entityName, long unsigned int jointId,
        const std::string& jointName) {
      //iterate over all robots to find the robot with the given name
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == entityName) {
          MutexLocker locker(&iMutex);
          iter->second->addJoint(jointId, jointName);
          break;
        }
      }
    }

    void EntityManager::addController(const std::string& entityName,
        long unsigned int controllerId) {
      //iterate over all robots to find the robot with the given name
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == entityName) {
          MutexLocker locker(&iMutex);
          iter->second->addController(controllerId);
          break;
        }
      }
    }

    void EntityManager::selectEvent(long unsigned int id, bool mode) {
      //the node was selected
      if (true == mode) {
        //go over all robots as we don't know which robot the node belongs to
        for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
            iter != entities.end(); ++iter) {
          //select returns true if the node belongs to the robot
          if (iter->second != NULL && iter->second->select(id)) {
            //TODO <jonas.peter@dfki.de> notify about selection change only if new selection
            std::cout << "robot has been selected: " << iter->second->getName() << std::endl;
            //TODO <jonas.peter@dfki.de> notify clients about selection change
          }
        }
      }
      //TODO <jonas.peter@dfki.de> handle deselection
    }

    SimEntity* EntityManager::getEntity(long unsigned int id) {
      //TODO replace with find
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->first == id) {
          return iter->second;
        }
      }
      return 0;
    }

    SimEntity* EntityManager::getEntity(const std::string& name) {
      return getEntity(name, true);
    }

    SimEntity* EntityManager::getEntity(const std::string& name, bool verbose) {
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == name) {
          return iter->second;
        }
      }
      if (verbose)
        fprintf(stderr, "ERROR: Entity with name %s not found!\n", name.c_str());
      return 0;
    }

    std::vector<SimEntity*> EntityManager::getEntities(const std::string &name) {
      std::vector<SimEntity*> out;
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (matchPattern(name, iter->second->getName())) {
          out.push_back(iter->second);
        }
      }
      return out;
    }

    std::vector<SimEntity*> EntityManager::getEntitiesOfAssembly(
      const std::string &assembly_name)
    {
      std::vector<SimEntity*> out;
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (matchPattern(assembly_name, iter->second->getAssembly())) {
          out.push_back(iter->second);
        }
      }
      return out;
    }

    SimEntity* EntityManager::getRootOfAssembly(
      const std::string &assembly_name)
    {
      std::vector<SimEntity*> out;
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (matchPattern(assembly_name, iter->second->getAssembly())) {
          configmaps::ConfigMap map = iter->second->getConfig();
          if (map.hasKey("root") && (bool) map["root"]) return iter->second;
        }
      }
      return 0;
    }

    SimEntity* EntityManager::getMainEntityOfAssembly(
      const std::string &assembly_name)
    {
      std::vector<SimEntity*> out;
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (matchPattern(assembly_name, iter->second->getAssembly())) {
          configmaps::ConfigMap map = iter->second->getConfig();
          if (map.hasKey("main_entity") && (bool) map["main_entity"]) return iter->second;
        }
      }
      return getRootOfAssembly(assembly_name);
    }

    long unsigned int EntityManager::getEntityNode(const std::string& entityName,
        const std::string& nodeName) {
      MutexLocker locker(&iMutex);
      SimEntity *entity = getEntity(entityName);
      unsigned long node = 0;
      if (entity) {
        node = entity->getNode(nodeName);
      }
      return node;
    }

    long unsigned int EntityManager::getEntityMotor(const std::string& entityName,
        const std::string& motorName) {
      //not sure if a mutex lock is needed here
      MutexLocker locker(&iMutex);
      SimEntity *entity = getEntity(entityName);
      unsigned long motor = 0;
      if (entity) {
        motor = entity->getMotor(motorName);
      }
      return motor;
    }

    std::vector<unsigned long> EntityManager::getEntityControllerList(
        const std::string &entityName) {
      //not sure if a mutex lock is needed here
      MutexLocker locker(&iMutex);
      SimEntity *entity = getEntity(entityName);
      if (entity) {
        return entity->getControllerList();
      }
      return std::vector<unsigned long>();
    }

    long unsigned int EntityManager::getEntityJoint(const std::string& entityName,
        const std::string& jointName) {
      //not sure if a mutex lock is needed here
      MutexLocker locker(&iMutex);
      SimEntity *entity = getEntity(entityName);
      unsigned long joint = 0;
      if (entity) {
        joint = entity->getJoint(jointName);
      }
      return joint;
    }

    void EntityManager::printEntityNodes(const std::string& entityName) {
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == entityName) {
          std::cout << "printing entity with id: " << iter->first << std::endl;
          iter->second->printNodes();
          break;
        }
      }
    }

    void EntityManager::printEntityMotors(const std::string& entityName) {
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == entityName) {
          std::cout << "printing entity with id: " << iter->first << std::endl;
          iter->second->printMotors();
          break;
        }
      }
    }

    void EntityManager::printEntityControllers(const std::string& entityName) {
      for (std::map<unsigned long, SimEntity*>::iterator iter = entities.begin();
          iter != entities.end(); ++iter) {
        if (iter->second->getName() == entityName) {
          std::cout << "printing entity with id: " << iter->first << std::endl;
          iter->second->printControllers();
          break;
        }
      }
    }

    void EntityManager::resetPose() {
      for (auto iter: entities) {
        iter.second->removeAnchor();
      }
      for (auto iter: entities) {
        iter.second->setInitialPose(true);
      }
    }

  } // end of namespace sim
} // end of namespace mars
