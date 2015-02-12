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


// http://www.postgresql.org/docs/9.1/static/lo-interfaces.html#AEN33102

#include "PostgreSQLLargeObject.h"

#include "PostgreSQLException.h"

#include <boost/lexical_cast.hpp>
#include <libpq/libpq-fs.h>


namespace OrthancPlugins
{  
  void PostgreSQLLargeObject::Create()
  {
    PGconn* pg = reinterpret_cast<PGconn*>(connection_.pg_);

    oid_ = lo_creat(pg, INV_WRITE);
    if (oid_ == 0)
    {
      throw PostgreSQLException("Cannot create a large object");
    }
  }


  void PostgreSQLLargeObject::Write(const void* data, 
                                    size_t size)
  {
    static int MAX_CHUNK_SIZE = 16 * 1024 * 1024;

    PGconn* pg = reinterpret_cast<PGconn*>(connection_.pg_);

    int fd = lo_open(pg, oid_, INV_WRITE);
    if (fd < 0)
    {
      throw PostgreSQLException();
    }

    const char* position = reinterpret_cast<const char*>(data);
    while (size > 0)
    {
      int chunk = (size > static_cast<size_t>(MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : static_cast<int>(size));
      int nbytes = lo_write(pg, fd, position, chunk);
      if (nbytes <= 0)
      {
        lo_close(pg, fd);
        throw PostgreSQLException();
      }

      size -= nbytes;
      position += nbytes;
    }

    lo_close(pg, fd);
  }


  PostgreSQLLargeObject::PostgreSQLLargeObject(PostgreSQLConnection& connection,
                                               const void* data,
                                               size_t size) : 
    connection_(connection)
  {
    Create();
    Write(data, size);
  }


  PostgreSQLLargeObject::PostgreSQLLargeObject(PostgreSQLConnection& connection,
                                               const std::string& s) : 
    connection_(connection)
  {
    Create();

    if (s.size() != 0)
    {
      Write(s.c_str(), s.size());
    }
    else
    {
      Write(NULL, 0);
    }
  }


  class PostgreSQLLargeObject::Reader
  {
  private: 
    PGconn* pg_;
    int fd_;
    size_t size_;

  public:
    Reader(PostgreSQLConnection& connection,
           const std::string& oid)
    {
      pg_ = reinterpret_cast<PGconn*>(connection.pg_);
      Oid id = boost::lexical_cast<Oid>(oid);

      fd_ = lo_open(pg_, id, INV_READ);

      if (fd_ < 0 ||
          lo_lseek(pg_, fd_, 0, SEEK_END) < 0)
      {
        throw PostgreSQLException("No such large object in the connection; Make sure you use a transaction");
      }

      // Get the size of the large object
      int size = lo_tell(pg_, fd_);
      if (size < 0)
      {
        throw PostgreSQLException("Internal error");
      }
      size_ = static_cast<size_t>(size);

      // Go to the first byte of the object
      lo_lseek(pg_, fd_, 0, SEEK_SET);
    }

    ~Reader()
    {
      lo_close(pg_, fd_);
    }

    size_t GetSize() const
    {
      return size_;
    }

    void Read(char* target)
    {
      for (size_t position = 0; position < size_; )
      {
        size_t remaining = size_ - position;
        size_t nbytes = lo_read(pg_, fd_, target + position, remaining);

        if (nbytes < 0)
        {
          throw PostgreSQLException("Unable to read the large object in the database");
        }

        position += nbytes;
      }
    }
  };
  

  void PostgreSQLLargeObject::Read(std::string& target,
                                   PostgreSQLConnection& connection,
                                   const std::string& oid)
  {
    Reader reader(connection, oid);
    target.resize(reader.GetSize());    

    if (target.size() > 0)
    {
      reader.Read(&target[0]);
    }
  }


  void PostgreSQLLargeObject::Read(void*& target,
                                   size_t& size,
                                   PostgreSQLConnection& connection,
                                   const std::string& oid)
  {
    Reader reader(connection, oid);
    size = reader.GetSize();

    if (size == 0)
    {
      target = NULL;
    }
    else
    {
      target = malloc(size);
      reader.Read(reinterpret_cast<char*>(target));
    }
  }


  std::string PostgreSQLLargeObject::GetOid() const
  {
    return boost::lexical_cast<std::string>(oid_);
  }


  void PostgreSQLLargeObject::Delete(PostgreSQLConnection& connection,
                                     const std::string& oid)
  {
    PGconn* pg = reinterpret_cast<PGconn*>(connection.pg_);
    Oid id = boost::lexical_cast<Oid>(oid);

    if (lo_unlink(pg, id) < 0)
    {
      throw PostgreSQLException("Unable to delete the large object from the database");
    }
  }
}
