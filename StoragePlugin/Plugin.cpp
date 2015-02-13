/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include <orthanc/OrthancCPlugin.h>

#include "PostgreSQLStorageArea.h"
#include "../Core/PostgreSQLException.h"
#include "../Core/Configuration.h"


static OrthancPluginContext* context_ = NULL;
static OrthancPlugins::PostgreSQLStorageArea* storage_ = NULL;


static int32_t StorageCreate(const char* uuid,
                             const void* content,
                             int64_t size,
                             OrthancPluginContentType type)
{
  try
  {
    storage_->Create(uuid, content, static_cast<size_t>(size), type);
    return 0;
  }
  catch (std::runtime_error& e)
  {
    OrthancPluginLogError(context_, e.what());
    return -1;
  }
}


static int32_t StorageRead(void** content,
                           int64_t* size,
                           const char* uuid,
                           OrthancPluginContentType type)
{
  try
  {
    size_t tmp;
    storage_->Read(*content, tmp, uuid, type);
    *size = static_cast<int64_t>(tmp);
    return 0;
  }
  catch (std::runtime_error& e)
  {
    OrthancPluginLogError(context_, e.what());
    return -1;
  }
}


static int32_t StorageRemove(const char* uuid,
                             OrthancPluginContentType type)
{
  try
  {
    storage_->Remove(uuid, type);
    return 0;
  }
  catch (std::runtime_error& e)
  {
    OrthancPluginLogError(context_, e.what());
    return -1;
  }
}



extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    context_ = context;
    OrthancPluginLogWarning(context_, "Using PostgreSQL storage area");

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context_) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context_->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context_, info);
      return -1;
    }

    bool allowUnlock = OrthancPlugins::IsFlagInCommandLineArguments(context_, FLAG_UNLOCK);

    OrthancPluginSetDescription(context_, "Stores the files received by Orthanc into a PostgreSQL database.");

    try
    {
      /* Create the connection to PostgreSQL */
      bool useLock;
      std::auto_ptr<OrthancPlugins::PostgreSQLConnection> pg(OrthancPlugins::CreateConnection(useLock, context_));
      pg->Open();
      //pg->ClearAll();   // Reset the database

      /* Create the storage area back-end */
      storage_ = new OrthancPlugins::PostgreSQLStorageArea(pg.release(), useLock, allowUnlock);

      /* Register the storage area into Orthanc */
      OrthancPluginRegisterStorageArea(context_, StorageCreate, StorageRead, StorageRemove);
    }
    catch (std::runtime_error& e)
    {
      OrthancPluginLogError(context_, e.what());
      return -1;
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPluginLogWarning(context_, "Storage plugin is finalizing");

    if (storage_ != NULL)
    {
      delete storage_;
      storage_ = NULL;
    }
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "postgresql-storage";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "1.0";
  }
}
