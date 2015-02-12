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


#include "PostgreSQLTransaction.h"

#include "PostgreSQLException.h"

namespace OrthancPlugins
{
  PostgreSQLTransaction::PostgreSQLTransaction(PostgreSQLConnection& connection,
                                               bool open) :
    connection_(connection),
    isOpen_(false)
  {
    if (open)
    {
      Begin();
    }
  }

  PostgreSQLTransaction::~PostgreSQLTransaction()
  {
    if (isOpen_)
    {
      connection_.Execute("ABORT");
    }
  }

  void PostgreSQLTransaction::Begin()
  {
    if (isOpen_) 
    {
      throw PostgreSQLException("PostgreSQL: Beginning a transaction twice!");
    }

    connection_.Execute("BEGIN");
    isOpen_ = true;
  }

  void PostgreSQLTransaction::Rollback() 
  {
    if (!isOpen_) 
    {
      throw PostgreSQLException("Attempting to rollback a nonexistent transaction. "
                                "Did you remember to call Begin()?");
    }

    connection_.Execute("ABORT");
    isOpen_ = false;
  }

  void PostgreSQLTransaction::Commit() 
  {
    if (!isOpen_) 
    {
      throw PostgreSQLException("Attempting to roll back a nonexistent transaction. "
                                "Did you remember to call Begin()?");
    }

    connection_.Execute("COMMIT");
    isOpen_ = false;
  }
}
