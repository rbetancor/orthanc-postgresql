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


#include "PostgreSQLResult.h"

#include "PostgreSQLException.h"

#include <cassert>
#include <boost/lexical_cast.hpp>

// PostgreSQL includes
#include <libpq-fe.h>
#include <c.h>
#include <catalog/pg_type.h>


namespace OrthancPlugins
{
  void PostgreSQLResult::Clear()
  {
    if (result_ != NULL)
    {
      PQclear(reinterpret_cast<PGresult*>(result_));
      result_ = NULL;
    }
  }

  void PostgreSQLResult::CheckDone()
  {
    if (position_ >= PQntuples(reinterpret_cast<PGresult*>(result_)))
    {
      // We are at the end of the result set
      Clear();
    }
  }

  void PostgreSQLResult::CheckColumn(unsigned int column, unsigned int /*Oid*/ expectedType) const
  {
    if (IsDone())
    {
      throw PostgreSQLException("Bad sequence of calls");
    }

    if (column >= static_cast<unsigned int>(PQnfields(reinterpret_cast<PGresult*>(result_))))
    {
      throw PostgreSQLException("Parameter out of range");
    }

    if (expectedType != 0 &&
        expectedType != PQftype(reinterpret_cast<PGresult*>(result_), column))
    {
      throw PostgreSQLException("Bad type of parameter");
    }
  }

  PostgreSQLResult::PostgreSQLResult(PostgreSQLStatement& statement) : 
    position_(0), 
    connection_(statement.GetConnection())
  {
    result_ = statement.Execute();
    assert(result_ != NULL);   // An exception would have been thrown otherwise

    // This is the first call to "Step()"
    if (PQresultStatus(reinterpret_cast<PGresult*>(result_)) != PGRES_TUPLES_OK)
    {
      throw PostgreSQLException("PostgreSQL: Step() applied to non-SELECT request");
    }

    CheckDone();
  }

  void PostgreSQLResult::Step()
  {
    position_++;
    CheckDone();
  }

  bool PostgreSQLResult::IsNull(unsigned int column) const
  {
    CheckColumn(column, 0);
    return PQgetisnull(reinterpret_cast<PGresult*>(result_), position_, column) != 0;
  }

  int PostgreSQLResult::GetInteger(unsigned int column) const
  {
    CheckColumn(column, INT4OID);
    assert(PQfsize(reinterpret_cast<PGresult*>(result_), column) == 4);
    char *v = PQgetvalue(reinterpret_cast<PGresult*>(result_), position_, column);
    return htobe32(*reinterpret_cast<int32_t*>(v));
  }

  int64_t PostgreSQLResult::GetInteger64(unsigned int column) const
  {
    CheckColumn(column, INT8OID);
    assert(PQfsize(reinterpret_cast<PGresult*>(result_), column) == 8);
    char *v = PQgetvalue(reinterpret_cast<PGresult*>(result_), position_, column);
    return htobe64(*reinterpret_cast<int64_t*>(v));
  }

  std::string PostgreSQLResult::GetString(unsigned int column) const
  {
    CheckColumn(column, 0);

    Oid oid = PQftype(reinterpret_cast<PGresult*>(result_), column);
    if (oid != TEXTOID && oid != VARCHAROID && oid != BYTEAOID)
    {
      throw PostgreSQLException("Bad type of parameter");
    }

    return std::string(PQgetvalue(reinterpret_cast<PGresult*>(result_), position_, column));
  }


  void PostgreSQLResult::GetLargeObject(std::string& result,
                                        unsigned int column) const
  {
    CheckColumn(column, OIDOID);

    Oid oid;
    assert(PQfsize(reinterpret_cast<PGresult*>(result_), column) == sizeof(oid));

    oid = *(const Oid*) PQgetvalue(reinterpret_cast<PGresult*>(result_), position_, column);
    oid = ntohl(oid);

    PostgreSQLLargeObject::Read(result, connection_, boost::lexical_cast<std::string>(oid));
  }


  void PostgreSQLResult::GetLargeObject(void*& result,
                                        size_t& size,
                                        unsigned int column) const
  {
    CheckColumn(column, OIDOID);

    Oid oid;
    assert(PQfsize(reinterpret_cast<PGresult*>(result_), column) == sizeof(oid));

    oid = *(const Oid*) PQgetvalue(reinterpret_cast<PGresult*>(result_), position_, column);
    oid = ntohl(oid);

    PostgreSQLLargeObject::Read(result, size, connection_, boost::lexical_cast<std::string>(oid));
  }
}
