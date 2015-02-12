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


#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#  if defined(_MSC_VER)
//   http://msdn.microsoft.com/en-us/library/a3140177.aspx
#    define htobe32(x) _byteswap_ulong(x)
#    define htobe64(x) _byteswap_uint64(x)
#  else   // MinGW
#    define htobe32(x) __builtin_bswap32(x)
#    define htobe64(x) __builtin_bswap64(x)
#  endif
#endif

#include <string>
#include <boost/noncopyable.hpp>
#include <stdint.h>

namespace OrthancPlugins
{
  class PostgreSQLConnection : public boost::noncopyable
  {
  private:
    friend class PostgreSQLStatement;
    friend class PostgreSQLLargeObject;

    std::string host_;
    uint16_t port_;
    std::string username_;
    std::string password_;
    std::string database_;
    void* pg_;   /* Object of type "PGconn*" */

    void Close();

  public:
    PostgreSQLConnection();

    PostgreSQLConnection(const PostgreSQLConnection& other);

    ~PostgreSQLConnection()
    {
      Close();
    }

    void SetHost(const std::string& host);

    const std::string& GetHost() const
    {
      return host_;
    }

    void SetPortNumber(uint16_t port);

    uint16_t GetPortNumber() const
    {
      return port_;
    }

    void SetUsername(const std::string& username);

    const std::string& GetUsername() const
    {
      return username_;
    }

    void SetPassword(const std::string& password);

    const std::string& GetPassword() const
    {
      return password_;
    }

    void SetDatabase(const std::string& database);

    void ResetDatabase()
    {
      SetDatabase("");
    }

    const std::string& GetDatabase() const
    {
      return database_;
    }

    void Open();

    void Execute(const std::string& sql);

    bool DoesTableExist(const char* name);

    void ClearAll();
  };
}
