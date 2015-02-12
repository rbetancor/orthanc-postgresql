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

#include <stdexcept>
#include <string>

namespace OrthancPlugins
{
  class PostgreSQLException : public std::runtime_error
  {
  public:
    PostgreSQLException() : 
      std::runtime_error("Error in PostgreSQL")
    {
    }

    PostgreSQLException(const std::string& message) : 
      std::runtime_error("Error in PostgreSQL: " + message)
    {
    }
  };
}
