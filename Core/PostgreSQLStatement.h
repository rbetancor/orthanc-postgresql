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

#include "PostgreSQLConnection.h"
#include "PostgreSQLLargeObject.h"

#include <vector>
#include <memory>
#include <boost/shared_ptr.hpp>

namespace OrthancPlugins
{
  class PostgreSQLStatement : public boost::noncopyable
  {
  private:
    class Inputs;
    friend class PostgreSQLResult;

    PostgreSQLConnection& connection_;
    std::string id_;
    std::string sql_;
    std::vector<unsigned int /*Oid*/>  oids_;
    std::vector<int>  binary_;
    boost::shared_ptr<Inputs> inputs_;

    void Prepare();

    void Unprepare();

    void DeclareInputInternal(unsigned int param,
                              unsigned int /*Oid*/ type);

    void* /* PGresult* */ Execute();

  public:
    PostgreSQLStatement(PostgreSQLConnection& connection,
                        const std::string& sql);

    ~PostgreSQLStatement()
    {
      Unprepare();
    }
    
    void DeclareInputInteger(unsigned int param);
    
    void DeclareInputInteger64(unsigned int param);

    void DeclareInputString(unsigned int param);

    void DeclareInputBinary(unsigned int param);

    void DeclareInputLargeObject(unsigned int param);

    void Run();

    void BindNull(unsigned int param);

    void BindInteger(unsigned int param, int value);

    void BindInteger64(unsigned int param, int64_t value);

    void BindString(unsigned int param, const std::string& value);

    void BindLargeObject(unsigned int param, const PostgreSQLLargeObject& value);

    PostgreSQLConnection& GetConnection() const
    {
      return connection_;
    }
  };
}
