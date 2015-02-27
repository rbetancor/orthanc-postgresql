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

#include "PostgreSQLStatement.h"

namespace OrthancPlugins
{
  class PostgreSQLResult : public boost::noncopyable
  {
  private:
    void *result_;  /* Object of type "PGresult*" */
    int position_;
    PostgreSQLConnection& connection_;

    void Clear();

    void CheckDone();

    void CheckColumn(unsigned int column, /*Oid*/ unsigned int expectedType) const;

  public:
    PostgreSQLResult(PostgreSQLStatement& statement);

    ~PostgreSQLResult()
    {
      Clear();
    }

    void Step();

    bool IsDone() const
    {
      return result_ == NULL;
    }

    bool IsNull(unsigned int column) const;

    bool GetBoolean(unsigned int column) const;

    int GetInteger(unsigned int column) const;

    int64_t GetInteger64(unsigned int column) const;

    std::string GetString(unsigned int column) const;

    void GetLargeObject(std::string& result,
                        unsigned int column) const;

    void GetLargeObject(void*& result,
                        size_t& size,
                        unsigned int column) const;
  };
}
