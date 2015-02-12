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
#include "PostgreSQLStatement.h"

#include <stdint.h>
#include <memory>

namespace OrthancPlugins
{
  class GlobalProperties
  {
  private:
    PostgreSQLConnection& connection_;
    int32_t lockKey_;

    std::auto_ptr<PostgreSQLStatement> lookupGlobalProperty_;
    std::auto_ptr<PostgreSQLStatement> setGlobalProperty_;

  public:
    GlobalProperties(PostgreSQLConnection& connection,
                     int32_t lockKey);

    void Lock(bool allowUnlock);

    void Unlock();

    bool LookupGlobalProperty(std::string& target,
                              int32_t property);

    void SetGlobalProperty(int32_t property,
                         const char* value);

  };
}
