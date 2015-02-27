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

#define USE_ADVISORY_LOCK  1

namespace OrthancPlugins
{
  GlobalProperties::GlobalProperties(PostgreSQLConnection& connection,
                                     bool useLock,
                                     int32_t lockKey) :
    connection_(connection),
    useLock_(useLock),
    lockKey_(lockKey)
  {
    PostgreSQLTransaction transaction(connection_);

    if (!connection_.DoesTableExist("GlobalProperties"))
    {
      connection_.Execute("CREATE TABLE GlobalProperties("
                          "property INTEGER PRIMARY KEY,"
                          "value TEXT)");
    }

    transaction.Commit();
  }


  void GlobalProperties::Lock(bool allowUnlock)
  {
    if (useLock_)
    {
      PostgreSQLTransaction transaction(connection_);

#if USE_ADVISORY_LOCK == 1
      PostgreSQLStatement s(connection_, "select pg_try_advisory_lock($1);");
      s.DeclareInputInteger(0);
      s.BindInteger(0, lockKey_);

      PostgreSQLResult result(s);
      if (result.IsDone() ||
          !result.GetBoolean(0))
      {
        throw PostgreSQLException("The database is locked by another instance of Orthanc.");
      }
#else
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
#endif

      transaction.Commit();
    }
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
    if (setGlobalProperty1_.get() == NULL ||
        setGlobalProperty2_.get() == NULL)
    {
      setGlobalProperty1_.reset
        (new PostgreSQLStatement
         (connection_, "DELETE FROM GlobalProperties WHERE property=$1"));
      setGlobalProperty1_->DeclareInputInteger(0);

      setGlobalProperty2_.reset
        (new PostgreSQLStatement
         (connection_, "INSERT INTO GlobalProperties VALUES ($1, $2)"));
      setGlobalProperty2_->DeclareInputInteger(0);
      setGlobalProperty2_->DeclareInputString(1);
    }

    setGlobalProperty1_->BindInteger(0, property);
    setGlobalProperty1_->Run();

    setGlobalProperty2_->BindInteger(0, property);
    setGlobalProperty2_->BindString(1, value);
    setGlobalProperty2_->Run();
  }


  void GlobalProperties::Unlock()
  {
    if (useLock_)
    {
#if USE_ADVISORY_LOCK == 1
      // Nothing to do, the lock is released after the connection is closed
#else
      // Remove the lock
      PostgreSQLTransaction transaction(connection_);
      SetGlobalProperty(lockKey_, "0");
      transaction.Commit();
#endif
    }
  }
}
