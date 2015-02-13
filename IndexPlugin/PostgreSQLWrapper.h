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

#include <orthanc/OrthancCppDatabasePlugin.h>

#include "../Core/GlobalProperties.h"
#include "../Core/PostgreSQLConnection.h"
#include "../Core/PostgreSQLStatement.h"
#include "../Core/PostgreSQLResult.h"
#include "../Core/PostgreSQLTransaction.h"

#include <list>

namespace OrthancPlugins
{
  class PostgreSQLWrapper : public IDatabaseBackend
  {
  private:
    std::auto_ptr<PostgreSQLConnection> connection_;
    std::auto_ptr<PostgreSQLTransaction>  transaction_;
    GlobalProperties  globalProperties_;

    std::auto_ptr<PostgreSQLStatement> attachFile_;
    std::auto_ptr<PostgreSQLStatement> attachChild_;
    std::auto_ptr<PostgreSQLStatement> createResource_;
    std::auto_ptr<PostgreSQLStatement> deleteAttachment_;
    std::auto_ptr<PostgreSQLStatement> deleteMetadata_;
    std::auto_ptr<PostgreSQLStatement> deleteResource_;
    std::auto_ptr<PostgreSQLStatement> getAllMetadata_;
    std::auto_ptr<PostgreSQLStatement> getAllPublicIds_;
    std::auto_ptr<PostgreSQLStatement> getChanges_;
    std::auto_ptr<PostgreSQLStatement> getLastChange_;
    std::auto_ptr<PostgreSQLStatement> getChildrenInternalId_;
    std::auto_ptr<PostgreSQLStatement> getChildrenPublicId_;
    std::auto_ptr<PostgreSQLStatement> getExports_;
    std::auto_ptr<PostgreSQLStatement> getLastExport_;
    std::auto_ptr<PostgreSQLStatement> getMainDicomTags1_;
    std::auto_ptr<PostgreSQLStatement> getMainDicomTags2_;
    std::auto_ptr<PostgreSQLStatement> getPublicId_;
    std::auto_ptr<PostgreSQLStatement> getResourceCount_;
    std::auto_ptr<PostgreSQLStatement> getResourceType_;
    std::auto_ptr<PostgreSQLStatement> getTotalCompressedSize_;
    std::auto_ptr<PostgreSQLStatement> getTotalUncompressedSize_;
    std::auto_ptr<PostgreSQLStatement> isProtectedPatient_;
    std::auto_ptr<PostgreSQLStatement> listMetadata_;
    std::auto_ptr<PostgreSQLStatement> listAttachments_;
    std::auto_ptr<PostgreSQLStatement> logChange_;
    std::auto_ptr<PostgreSQLStatement> logExport_;
    std::auto_ptr<PostgreSQLStatement> lookupAttachment_;
    std::auto_ptr<PostgreSQLStatement> lookupIdentifier1_;
    std::auto_ptr<PostgreSQLStatement> lookupIdentifier2_;
    std::auto_ptr<PostgreSQLStatement> lookupMetadata_;
    std::auto_ptr<PostgreSQLStatement> lookupParent_;
    std::auto_ptr<PostgreSQLStatement> lookupResource_;
    std::auto_ptr<PostgreSQLStatement> selectPatientToRecycle_;
    std::auto_ptr<PostgreSQLStatement> selectPatientToRecycleAvoid_;
    std::auto_ptr<PostgreSQLStatement> setMainDicomTags_;
    std::auto_ptr<PostgreSQLStatement> setIdentifierTag_;
    std::auto_ptr<PostgreSQLStatement> setMetadata1_;
    std::auto_ptr<PostgreSQLStatement> setMetadata2_;
    std::auto_ptr<PostgreSQLStatement> protectPatient1_;
    std::auto_ptr<PostgreSQLStatement> protectPatient2_;

    std::auto_ptr<PostgreSQLStatement> clearDeletedFiles_;
    std::auto_ptr<PostgreSQLStatement> clearDeletedResources_;
    std::auto_ptr<PostgreSQLStatement> clearRemainingAncestor_;
    std::auto_ptr<PostgreSQLStatement> getDeletedFiles_;
    std::auto_ptr<PostgreSQLStatement> getDeletedResources_;
    std::auto_ptr<PostgreSQLStatement> getRemainingAncestor_;
 
    void Prepare();

    void SignalDeletedFilesAndResources();

    void GetChangesInternal(bool& done,
                            PostgreSQLStatement& s,
                            uint32_t maxResults);

    void GetExportedResourcesInternal(bool& done,
                                      PostgreSQLStatement& s,
                                      uint32_t maxResults);

    void ClearTable(const std::string& tableName);

  public:
    PostgreSQLWrapper(PostgreSQLConnection* connection,  // Takes the ownership of the connection
                      bool allowUnlock);

    virtual ~PostgreSQLWrapper();

    virtual void Open()
    {
      connection_->Open();
    }

    virtual void Close()
    {
      transaction_.reset(NULL);
    }

    virtual void AddAttachment(int64_t id,
                               const OrthancPluginAttachment& attachment);

    virtual void AttachChild(int64_t parent,
                             int64_t child);

    virtual void ClearChanges()
    {
      ClearTable("Changes");
    }

    virtual void ClearExportedResources()
    {
      ClearTable("ExportedResources");
    }

    virtual int64_t CreateResource(const char* publicId,
                                   OrthancPluginResourceType type);

    virtual void DeleteAttachment(int64_t id,
                                  int32_t attachment);

    virtual void DeleteMetadata(int64_t id,
                                int32_t type);

    virtual void DeleteResource(int64_t id);

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 OrthancPluginResourceType resourceType);

    virtual void GetChanges(bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults);

    virtual void GetChildrenInternalId(std::list<int64_t>& result,
                                       int64_t id);

    virtual void GetChildrenPublicId(std::list<std::string>& result,
                                     int64_t id);

    virtual void GetExportedResources(bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults);

    virtual void GetLastChange();

    virtual void GetLastExportedResource();
    
    virtual void GetMainDicomTags(int64_t id);

    virtual std::string GetPublicId(int64_t resourceId);

    virtual uint64_t GetResourceCount(OrthancPluginResourceType resourceType);

    virtual OrthancPluginResourceType GetResourceType(int64_t resourceId);

    virtual uint64_t GetTotalCompressedSize();
    
    virtual uint64_t GetTotalUncompressedSize();

    virtual bool IsExistingResource(int64_t internalId);

    virtual bool IsProtectedPatient(int64_t internalId);

    virtual void ListAvailableMetadata(std::list<int32_t>& target,
                                       int64_t id);

    virtual void ListAvailableAttachments(std::list<int32_t>& result,
                                          int64_t id);

    virtual void LogChange(const OrthancPluginChange& change);

    virtual void LogExportedResource(const OrthancPluginExportedResource& resource);
    
    virtual bool LookupAttachment(int64_t id,
                                  int32_t contentType);

    virtual bool LookupGlobalProperty(std::string& target,
                                      int32_t property)
    {
      return globalProperties_.LookupGlobalProperty(target, property);
    }

    virtual void LookupIdentifier(std::list<int64_t>& result,
                                  uint16_t group,
                                  uint16_t element,
                                  const char* value);

    virtual void LookupIdentifier(std::list<int64_t>& result,
                                  const char* value);

    virtual bool LookupMetadata(std::string& target,
                                int64_t id,
                                int32_t type);

    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId);

    virtual bool LookupResource(int64_t& id /*out*/,
                                OrthancPluginResourceType& type /*out*/,
                                const char* publicId);

    virtual bool SelectPatientToRecycle(int64_t& internalId);

    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid);

    virtual void SetGlobalProperty(int32_t property,
                                   const char* value)
    {
      return globalProperties_.SetGlobalProperty(property, value);
    }

    virtual void SetMainDicomTag(int64_t id,
                                 uint16_t group,
                                 uint16_t element,
                                 const char* value);

    virtual void SetIdentifierTag(int64_t id,
                                  uint16_t group,
                                  uint16_t element,
                                  const char* value);

    virtual void SetMetadata(int64_t id,
                             int32_t type,
                             const char* value);

    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected);

    virtual void StartTransaction()
    {
      transaction_.reset(new PostgreSQLTransaction(*connection_));
    }

    virtual void RollbackTransaction()
    {
      transaction_.reset(NULL);
    }

    virtual void CommitTransaction()
    {
      transaction_->Commit();
      transaction_.reset(NULL);
    }

    // For unit tests only!
    void GetChildren(std::list<std::string>& childrenPublicIds,
                     int64_t id);

    // For unit tests only!
    int64_t GetTableRecordCount(const std::string& table);

    // For unit tests only!
    bool GetParentPublicId(std::string& result,
                           int64_t id);
  };
}
