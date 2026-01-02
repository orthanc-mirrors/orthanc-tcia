/**
 * TCIA plugin for Orthanc
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include <Compatibility.h>

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"


namespace OrthancPlugins
{
  class TciaImportJob : public OrthancJob
  {
  public:
    class Series
    {
    private:
      std::string   collection_;
      std::string   patientId_;
      std::string   seriesInstanceUid_;
      unsigned int  instancesCount_;
      uint64_t      size_;

    public:
      Series(const std::string& collection,
             const std::string& patientId,
             const std::string& seriesInstanceUid,
             unsigned int instancesCount,
             uint64_t size);

      const std::string& GetCollection() const
      {
        return collection_;
      }

      const std::string& GetPatientId() const
      {
        return patientId_;
      }

      const std::string& GetSeriesInstanceUid() const
      {
        return seriesInstanceUid_;
      }

      unsigned int GetInstancesCount() const
      {
        return instancesCount_;
      }

      uint64_t GetSize() const
      {
        return size_;
      }

      void Serialize(Json::Value& target) const;

      static Series Unserialize(const Json::Value& source);
    };

  private:
    std::vector<Series>  series_;
    size_t               position_;
    unsigned int         totalInstancesCount_;
    uint64_t             totalSize_;

    void AddSeriesInternal(const Series& series);
    
    void UpdateInfo();

  public:
    TciaImportJob();
    
    void Reserve(size_t count)
    {
      series_.reserve(count);
    }

    void AddSeries(const Series& series);
    
    void AddSeries(const std::string& collection,
                   const std::string& patientId,
                   const std::string& seriesInstanceUid,
                   unsigned int instancesCount,
                   uint64_t size);
    
    void AddNbiaClientSpreadsheet(const std::string& csv);
    
    virtual OrthancPluginJobStepStatus Step() ORTHANC_OVERRIDE;
    
    virtual void Stop(OrthancPluginJobStopReason reason) ORTHANC_OVERRIDE
    {
    }
    
    virtual void Reset() ORTHANC_OVERRIDE
    {
      position_ = 0;
    }

    static std::string GetJobType();
    
    static TciaImportJob* Unserialize(const Json::Value& serialized);
  };
}
