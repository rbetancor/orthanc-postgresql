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


#include "PostgreSQLConnection.h"

#include "PostgreSQLException.h"
#include "PostgreSQLResult.h"
#include "PostgreSQLStatement.h"
#include "PostgreSQLTransaction.h"

#include <boost/lexical_cast.hpp>

// PostgreSQL includes
#include <libpq-fe.h>
#include <c.h>
#include <catalog/pg_type.h>


namespace OrthancPlugins
{
  void PostgreSQLConnection::Close()
  {
    if (pg_ != NULL)
    {
      PQfinish(reinterpret_cast<PGconn*>(pg_));
      pg_ = NULL;
    }
  }

  
  PostgreSQLConnection::PostgreSQLConnection()
  {
    pg_ = NULL;
    host_ = "localhost";
    port_ = 5432;
    username_ = "postgres";
    password_ = "postgres";
    database_ = "";
    uri_.clear();
  }

  
  PostgreSQLConnection::PostgreSQLConnection(const PostgreSQLConnection& other) : 
    host_(other.host_),
    port_(other.port_),
    username_(other.username_),
    password_(other.password_),
    database_(other.database_),
    pg_(NULL)
  {
  }


  void PostgreSQLConnection::SetConnectionUri(const std::string& uri)
  {
    Close();
    uri_ = uri;
  }


  std::string PostgreSQLConnection::GetConnectionUri() const
  {
    if (uri_.empty())
    {
      return ("postgresql://" + username_ + ":" + password_ + "@" + 
              host_ + ":" + boost::lexical_cast<std::string>(port_) + "/" + database_);
    }
    else
    {
      return uri_;
    }
  }


  void PostgreSQLConnection::SetHost(const std::string& host)
  {
    Close();
    uri_.clear();
    host_ = host;
  }

  void PostgreSQLConnection::SetPortNumber(uint16_t port)
  {
    Close();
    uri_.clear();
    port_ = port;
  }

  void PostgreSQLConnection::SetUsername(const std::string& username)
  {
    Close();
    uri_.clear();
    username_ = username;
  }

  void PostgreSQLConnection::SetPassword(const std::string& password)
  {
    Close();
    uri_.clear();
    password_ = password;
  }

  void PostgreSQLConnection::SetDatabase(const std::string& database)
  {
    Close();
    uri_.clear();
    database_ = database;
  }

  void PostgreSQLConnection::Open()
  {
    if (pg_ != NULL)
    {
      // Already connected
      return;
    }

    std::string s;

    if (uri_.empty())
    {
      s = std::string("sslmode=disable") +   // TODO WHY SSL DOES NOT WORK? ("SSL error: wrong version number")
        " user=" + username_ + 
        " password=" + password_ + 
        " host=" + host_ + 
        " port=" + boost::lexical_cast<std::string>(port_);

      if (database_.size() > 0)
      {
        s += " dbname=" + database_;
      }
    }
    else
    {
      s = uri_;
    }

    pg_ = PQconnectdb(s.c_str());

    if (pg_ == NULL ||
        PQstatus(reinterpret_cast<PGconn*>(pg_)) != CONNECTION_OK)
    {
      std::string message;

      if (pg_)
      {
        message = PQerrorMessage(reinterpret_cast<PGconn*>(pg_));
        PQfinish(reinterpret_cast<PGconn*>(pg_));
        pg_ = NULL;
      }

      throw PostgreSQLException(message);
    }
  }


  void PostgreSQLConnection::Execute(const std::string& sql)
  {
    Open();

    PGresult* result = PQexec(reinterpret_cast<PGconn*>(pg_), sql.c_str());
    if (result == NULL)
    {
      throw PostgreSQLException(PQerrorMessage(reinterpret_cast<PGconn*>(pg_)));
    }

    bool ok = (PQresultStatus(result) == PGRES_COMMAND_OK ||
               PQresultStatus(result) == PGRES_TUPLES_OK);

    if (ok)
    {
      PQclear(result);
    }
    else
    {
      std::string message = PQresultErrorMessage(result);
      PQclear(result);
      throw PostgreSQLException(message);
    }
  }


  bool PostgreSQLConnection::DoesTableExist(const char* name)
  {
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), tolower);

    // http://stackoverflow.com/a/24089729/881731

    PostgreSQLStatement statement(*this, 
                                  "SELECT 1 FROM pg_catalog.pg_class c "
                                  "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
                                  "WHERE n.nspname = 'public' AND c.relkind='r' "
                                  "AND c.relname=$1");

    statement.DeclareInputString(0);
    statement.BindString(0, lower);

    PostgreSQLResult result(statement);
    return !result.IsDone();
  }



  void PostgreSQLConnection::ClearAll()
  {
    PostgreSQLTransaction transaction(*this);
    
    // Remove all the large objects
    Execute("SELECT lo_unlink(loid) FROM (SELECT DISTINCT loid FROM pg_catalog.pg_largeobject) as loids;");

    // http://stackoverflow.com/a/21247009/881731
    Execute("DROP SCHEMA public CASCADE;");
    Execute("CREATE SCHEMA public;");
    Execute("GRANT ALL ON SCHEMA public TO postgres;");
    Execute("GRANT ALL ON SCHEMA public TO public;");
    Execute("COMMENT ON SCHEMA public IS 'standard public schema';");

    transaction.Commit();
  }

}
