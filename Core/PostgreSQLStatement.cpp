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


#include "PostgreSQLStatement.h"

#include "PostgreSQLException.h"
#include "Configuration.h"

#include <cassert>

// PostgreSQL includes
#include <libpq-fe.h>
#include <c.h>
#include <catalog/pg_type.h>


namespace OrthancPlugins
{
  class PostgreSQLStatement::Inputs : public boost::noncopyable
  {
  private:
    std::vector<char*> values_;
    std::vector<int> sizes_;

    static char* Allocate(const void* source, int size)
    {
      if (size == 0)
      {
        return NULL;
      }
      else
      {
        char* ptr = reinterpret_cast<char*>(malloc(size));

        if (source != NULL)
        {
          memcpy(ptr, source, size);
        }

        return ptr;
      }
    }

    void Resize(size_t size)
    {
      // Shrinking of the vector
      for (size_t i = size; i < values_.size(); i++)
      {
        if (values_[i] != NULL)
          free(values_[i]);
      }

      values_.resize(size, NULL);
      sizes_.resize(size, 0);
    }

    void EnlargeForIndex(size_t index)
    {
      if (index >= values_.size())
      {
        // The vector is too small
        Resize(index + 1);
      }
    }

  public:
    Inputs()
    {
    }

    ~Inputs()
    {
      Resize(0);
    }

    void SetItem(size_t pos, const void* source, int size)
    {
      EnlargeForIndex(pos);

      if (sizes_[pos] == size)
      {
        if (source && size != 0)
        {
          memcpy(values_[pos], source, size);
        }
      }
      else
      {
        if (values_[pos] != NULL)
        {
          free(values_[pos]);
        }

        values_[pos] = Allocate(source, size);
        sizes_[pos] = size;
      }
    }

    void SetItem(size_t pos, int size)
    {
      SetItem(pos, NULL, size);
    }

    void* GetItem(size_t pos) const
    {
      if (pos >= values_.size())
      {
        throw PostgreSQLException("Parameter out of range");
      }

      return values_[pos];
    }

    const std::vector<char*>& GetValues() const
    {
      return values_;
    }

    const std::vector<int>& GetSizes() const
    {
      return sizes_;
    }
  };


  void PostgreSQLStatement::Prepare()
  {
    if (id_.size() > 0)
    {
      // Already prepared
      return;
    }

    for (size_t i = 0; i < oids_.size(); i++)
    {
      if (oids_[i] == 0)
      {
        // The type of an input parameter was not set
        throw PostgreSQLException();
      }
    }

    id_ = GenerateUuid();

    const unsigned int* tmp = oids_.size() ? &oids_[0] : NULL;

    PGresult* result = PQprepare(reinterpret_cast<PGconn*>(connection_.pg_),
                                 id_.c_str(), sql_.c_str(), oids_.size(), tmp);

    if (result == NULL)
    {
      id_.clear();
      throw PostgreSQLException(PQerrorMessage(reinterpret_cast<PGconn*>(connection_.pg_)));
    }

    bool ok = (PQresultStatus(result) == PGRES_COMMAND_OK);
    if (ok)
    {
      PQclear(result);
    }
    else
    {
      std::string message = PQresultErrorMessage(result);
      PQclear(result);
      id_.clear();
      throw PostgreSQLException(message);
    }
  }


  void PostgreSQLStatement::Unprepare()
  {
    if (id_.size() > 0)
    {
      // "Although there is no libpq function for deleting a
      // prepared statement, the SQL DEALLOCATE statement can be
      // used for that purpose."
      //connection_.Execute("DEALLOCATE " + id_);
    }

    id_.clear();
  }


  void PostgreSQLStatement::DeclareInputInternal(unsigned int param,
                                                 unsigned int /*Oid*/ type)
  {
    Unprepare();

    if (oids_.size() <= param)
    {
      oids_.resize(param + 1, 0);
      binary_.resize(param + 1);
    }

    oids_[param] = type;
    binary_[param] = (type == TEXTOID || type == BYTEAOID || type == OIDOID) ? 0 : 1;
  }


  void PostgreSQLStatement::DeclareInputInteger(unsigned int param)
  {
    DeclareInputInternal(param, INT4OID);
  }
    

  void PostgreSQLStatement::DeclareInputInteger64(unsigned int param)
  {
    DeclareInputInternal(param, INT8OID);
  }


  void PostgreSQLStatement::DeclareInputString(unsigned int param)
  {
    DeclareInputInternal(param, TEXTOID);
  }


  void PostgreSQLStatement::DeclareInputBinary(unsigned int param)
  {
    DeclareInputInternal(param, BYTEAOID);
  }


  void PostgreSQLStatement::DeclareInputLargeObject(unsigned int param)
  {
    DeclareInputInternal(param, OIDOID);
  }


  void* /* PGresult* */ PostgreSQLStatement::Execute()
  {
    Prepare();

    PGresult* result;

    if (oids_.size() == 0)
    {
      // No parameter
      result = PQexecPrepared(reinterpret_cast<PGconn*>(connection_.pg_),
                              id_.c_str(), 0, NULL, NULL, NULL, 1);
    }
    else
    {
      // At least 1 parameter
      result = PQexecPrepared(reinterpret_cast<PGconn*>(connection_.pg_),
                              id_.c_str(),
                              oids_.size(),
                              &inputs_->GetValues()[0],
                              &inputs_->GetSizes()[0],
                              &binary_[0],
                              1);
    }

    if (result == NULL)
    {
      throw PostgreSQLException(PQerrorMessage(reinterpret_cast<PGconn*>(connection_.pg_)));
    }

    return result;
  }


  PostgreSQLStatement::PostgreSQLStatement(PostgreSQLConnection& connection,
                                           const std::string& sql) :
    connection_(connection),
    sql_(sql),
    inputs_(new Inputs)
  {
    connection_.Open();
  }


  void PostgreSQLStatement::Run()
  {
    PGresult* result = reinterpret_cast<PGresult*>(Execute());
    assert(result != NULL);   // An exception would have been thrown otherwise

    bool ok = (PQresultStatus(result) == PGRES_COMMAND_OK ||
               PQresultStatus(result) == PGRES_TUPLES_OK);
    if (ok)
    {
      PQclear(result);
    }
    else
    {
      std::string error = PQresultErrorMessage(result);
      PQclear(result);
      throw PostgreSQLException(error);
    }
  }


  void PostgreSQLStatement::BindNull(unsigned int param)
  {
    if (param >= oids_.size())
    {
      throw PostgreSQLException("Parameter out of range");
    }

    inputs_->SetItem(param, 0);
  }


  void PostgreSQLStatement::BindInteger(unsigned int param, int value)
  {
    if (param >= oids_.size())
    {
      throw PostgreSQLException("Parameter out of range");
    }

    if (oids_[param] != INT4OID)
    {
      throw PostgreSQLException("Bad type of parameter");
    }

    assert(sizeof(int32_t) == 4);
    int32_t v = htobe32(static_cast<int32_t>(value));
    inputs_->SetItem(param, &v, sizeof(int32_t));
  }


  void PostgreSQLStatement::BindInteger64(unsigned int param, int64_t value)
  {
    if (param >= oids_.size())
    {
      throw PostgreSQLException("Parameter out of range");
    }

    if (oids_[param] != INT8OID)
    {
      throw PostgreSQLException("Bad type of parameter");
    }

    assert(sizeof(int64_t) == 8);
    int64_t v = htobe64(value);
    inputs_->SetItem(param, &v, sizeof(int64_t));
  }


  void PostgreSQLStatement::BindString(unsigned int param, const std::string& value)
  {
    if (param >= oids_.size())
    {
      throw PostgreSQLException("Parameter out of range");
    }

    if (oids_[param] != TEXTOID && oids_[param] != BYTEAOID)
    {
      throw PostgreSQLException("Bad type of parameter");
    }

    if (value.size() == 0)
    {
      inputs_->SetItem(param, "", 1 /* end-of-string character */);
    }
    else
    {
      inputs_->SetItem(param, value.c_str(), 
                       value.size() + 1);  // "+1" for end-of-string character
    }
  }


  void PostgreSQLStatement::BindLargeObject(unsigned int param, const PostgreSQLLargeObject& value)
  {
    if (param >= oids_.size())
    {
      throw PostgreSQLException("Parameter out of range");
    }

    if (oids_[param] != OIDOID)
    {
      throw PostgreSQLException("Bad type of parameter");
    }

    inputs_->SetItem(param, value.GetOid().c_str(), 
                     value.GetOid().size() + 1);  // "+1" for end-of-string character
  }
}
