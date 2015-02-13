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

#include "PostgreSQLWrapper.h"
#include "../Core/PostgreSQLException.h"
#include "../Core/Configuration.h"


static OrthancPluginContext* context_ = NULL;
static OrthancPlugins::PostgreSQLWrapper* backend_ = NULL;


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    context_ = context;
    OrthancPluginLogWarning(context_, "Using PostgreSQL index");

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

    OrthancPluginSetDescription(context_, "Stores the Orthanc index into a PostgreSQL database.");

    try
    {
      /* Create the connection to PostgreSQL */
      std::auto_ptr<OrthancPlugins::PostgreSQLConnection> pg(OrthancPlugins::CreateConnection(context_));
      pg->Open();
      //pg->ClearAll();   // Reset the database
 
      /* Create the database back-end */
      backend_ = new OrthancPlugins::PostgreSQLWrapper(pg.release(), allowUnlock);

      /* Register the PostgreSQL index into Orthanc */
      OrthancPlugins::DatabaseBackendAdapter::Register(context_, *backend_);
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
    OrthancPluginLogWarning(context_, "PostgreSQL index is finalizing");

    if (backend_ != NULL)
    {
      delete backend_;
      backend_ = NULL;
    }
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "postgresql-index";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "1.0";
  }
}
