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


#include "GlobalProperties.h"

#include "Configuration.h"
#include "PostgreSQLException.h"
#include "PostgreSQLResult.h"
#include "PostgreSQLTransaction.h"

#include "EmbeddedResources.h"

namespace OrthancPlugins
{
  GlobalProperties::GlobalProperties(PostgreSQLConnection& connection,
                                     int32_t lockKey) :
    connection_(connection),
    lockKey_(lockKey)
  {
    PostgreSQLTransaction transaction(connection_);

    if (!connection_.DoesTableExist("GlobalProperties"))
    {
      std::string query;

      EmbeddedResources::GetFileResource(query, EmbeddedResources::GLOBAL_PROPERTIES);
      connection_.Execute(query);
    }

    transaction.Commit();
  }


  void GlobalProperties::Lock(bool allowUnlock)
  {
    PostgreSQLTransaction transaction(connection_);

    // Check the lock
    if (!allowUnlock)
    {
      std::string lock = "0";
      if (LookupGlobalProperty(lock, lockKey_) &&
          lock != "0")
      {
        throw PostgreSQLException("The database is locked by another instance of Orthanc. "
                                  "Use \"" + FLAG_UNLOCK + "\" to manually remove the lock.");
      }
    }

    // Lock the database
    SetGlobalProperty(lockKey_, "1");             

    transaction.Commit();
  }


  bool GlobalProperties::LookupGlobalProperty(std::string& target,
                                              int32_t property)
  {
    if (lookupGlobalProperty_.get() == NULL)
    {
      lookupGlobalProperty_.reset
        (new PostgreSQLStatement
         (connection_, "SELECT value FROM GlobalProperties WHERE property=$1"));
      lookupGlobalProperty_->DeclareInputInteger(0);
    }

    lookupGlobalProperty_->BindInteger(0, static_cast<int>(property));

    PostgreSQLResult result(*lookupGlobalProperty_);
    if (result.IsDone())
    {
      return false;
    }
    else
    {
      target = result.GetString(0);
      return true;
    }
  }


  void GlobalProperties::SetGlobalProperty(int32_t property,
                                           const char* value)
  {
    if (setGlobalProperty_.get() == NULL)
    {
      // http://stackoverflow.com/a/1109198/881731
      setGlobalProperty_.reset
        (new PostgreSQLStatement
         (connection_, "SELECT ChangeGlobalProperty($1, $2)"));
      setGlobalProperty_->DeclareInputInteger(0);
      setGlobalProperty_->DeclareInputString(1);
    }

    setGlobalProperty_->BindInteger(0, property);
    setGlobalProperty_->BindString(1, value);
    setGlobalProperty_->Run();
  }


  void GlobalProperties::Unlock()
  {
    // Remove the lock
    PostgreSQLTransaction transaction(connection_);
    SetGlobalProperty(lockKey_, "0");
    transaction.Commit();
  }
}
