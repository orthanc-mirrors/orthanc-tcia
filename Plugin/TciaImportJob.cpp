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


#include "TciaImportJob.h"

#include "CsvParser.h"

#include <Logging.h>
#include <SerializationToolbox.h>
#include <Toolbox.h>


static const char* const COLLECTION = "Collection";
static const char* const INSTANCES_COUNT = "InstancesCount";
static const char* const JOB_TYPE = "TciaImportJob";
static const char* const ORTHANC_ID = "OrthancID";
static const char* const PATIENT_ID = "PatientID";
static const char* const SERIES = "Series";
static const char* const SERIES_INSTANCE_UID = "SeriesInstanceUID";
static const char* const SIZE = "Size";


namespace OrthancPlugins
{
  TciaImportJob::Series::Series(const std::string& collection,
                                const std::string& patientId,
                                const std::string& seriesInstanceUid,
                                unsigned int instancesCount,
                                uint64_t size) :
    collection_(collection),
    patientId_(patientId),
    seriesInstanceUid_(seriesInstanceUid),
    instancesCount_(instancesCount),
    size_(size)
  {
  }


  void TciaImportJob::Series::Serialize(Json::Value& target) const
  {
    target = Json::objectValue;
    target[COLLECTION] = collection_;
    target[PATIENT_ID] = patientId_;
    target[SERIES_INSTANCE_UID] = seriesInstanceUid_;
    target[INSTANCES_COUNT] = instancesCount_;
    target[SIZE] = boost::lexical_cast<std::string>(size_);
  }

  
  TciaImportJob::Series TciaImportJob::Series::Unserialize(const Json::Value& source)
  {
    uint64_t size;

    if (source.isMember(SIZE))
    {
      try
      {
        size = boost::lexical_cast<uint64_t>(Orthanc::SerializationToolbox::ReadString(source, SIZE));
      }
      catch (boost::bad_lexical_cast&)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat);
      }
    }
    else
    {
      size = 0;
    }

    return Series(Orthanc::SerializationToolbox::ReadString(source, COLLECTION),
                  Orthanc::SerializationToolbox::ReadString(source, PATIENT_ID),
                  Orthanc::SerializationToolbox::ReadString(source, SERIES_INSTANCE_UID),
                  Orthanc::SerializationToolbox::ReadUnsignedInteger(source, INSTANCES_COUNT),
                  size);
  }

  
  void TciaImportJob::AddSeriesInternal(const Series& series)
  {
    if (position_ != 0)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      series_.push_back(series);
      totalInstancesCount_ += series.GetInstancesCount();
      totalSize_ += series.GetSize();
    }
  }
  

  void TciaImportJob::UpdateInfo()
  {
    Json::Value series = Json::arrayValue;
    for (size_t i = 0; i < series_.size(); i++)
    {
      Json::Value item;
      series_[i].Serialize(item);
      series.append(item);
    }

    {
      Json::Value serialized = Json::objectValue;
      serialized[SERIES] = series;
      OrthancJob::UpdateSerialized(serialized);
    }

    for (size_t i = 0; i < series_.size(); i++)
    {
      std::string orthancId;
      Orthanc::Toolbox::ComputeSHA1(orthancId, series_[i].GetPatientId());
      series[static_cast<Json::Value::ArrayIndex>(i)][ORTHANC_ID] = orthancId;
    }
      
    {
      Json::Value content = Json::objectValue;
      content["Series"] = series;
      content["SeriesCount"] = static_cast<unsigned int>(series_.size());
      content["InstancesCount"] = totalInstancesCount_;
      content["Size"] = boost::lexical_cast<std::string>(totalSize_);
      content["SizeMB"] = static_cast<unsigned int>(totalSize_ / static_cast<uint64_t>((1024 * 1024)));
      OrthancJob::UpdateContent(content);
    }
  }


  TciaImportJob::TciaImportJob() :
    OrthancJob(JOB_TYPE),
    position_(0),
    totalInstancesCount_(0),
    totalSize_(0)
  {
  }
    

  void TciaImportJob::AddSeries(const std::string& collection,
                                const std::string& patientId,
                                const std::string& seriesInstanceUid,
                                unsigned int instancesCount,
                                uint64_t size)
  {
    AddSeriesInternal(Series(collection, patientId, seriesInstanceUid, instancesCount, size));
    UpdateInfo();
  }

  
  void TciaImportJob::AddSeries(const Series& series)
  {
    AddSeriesInternal(series);
    UpdateInfo();
  }

  
  void TciaImportJob::AddNbiaClientSpreadsheet(const std::string& csv)
  {
    static const char* const COLLECTION_NAME = "Collection Name";
    static const char* const SUBJECT_ID = "Subject ID";
    static const char* const SERIES_ID = "Series ID";
    static const char* const NUMBER_OF_IMAGES = "Number of images";
    static const char* const FILE_SIZE = "File Size (Bytes)";
      
    OrthancPlugins::CsvParser parser;
    parser.Parse(csv);

    std::map<std::string, size_t> index;
    parser.GetHeaderIndex(index);

    std::map<std::string, size_t>::const_iterator collectionName = index.find(COLLECTION_NAME);
    std::map<std::string, size_t>::const_iterator subjectId = index.find(SUBJECT_ID);
    std::map<std::string, size_t>::const_iterator seriesId = index.find(SERIES_ID);
    std::map<std::string, size_t>::const_iterator instancesCount = index.find(NUMBER_OF_IMAGES);
    std::map<std::string, size_t>::const_iterator size = index.find(FILE_SIZE);

    if (collectionName == index.end())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                      "Invalid TCIA cart, missing column: " + std::string(COLLECTION_NAME));
    }
    else if (subjectId == index.end())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                      "Invalid TCIA cart, missing column: " + std::string(SUBJECT_ID));
    }
    else if (seriesId == index.end())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                      "Invalid TCIA cart, missing column: " + std::string(SERIES_ID));
    }
    else if (instancesCount == index.end())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                      "Invalid TCIA cart, missing column: " + std::string(NUMBER_OF_IMAGES));
    }
    else if (size == index.end())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                      "Invalid TCIA cart, missing column: " + std::string(FILE_SIZE));
    }
    else
    {
      Reserve(series_.size() + parser.GetRowsCount() - 1);
        
      for (size_t i = 1; i < parser.GetRowsCount(); i++)
      {
        unsigned int instancesCount2;
        try
        {
          instancesCount2 = boost::lexical_cast<unsigned int>(parser.GetCell(i, instancesCount->second));
        }
        catch (boost::bad_lexical_cast&)
        {
          instancesCount2 = 0;
        }

        uint64_t size2;
        try
        {
          size2 = boost::lexical_cast<uint64_t>(parser.GetCell(i, size->second));
        }
        catch (boost::bad_lexical_cast&)
        {
          size2 = 0;
        }

        AddSeriesInternal(Series(parser.GetCell(i, collectionName->second),
                                 parser.GetCell(i, subjectId->second),
                                 parser.GetCell(i, seriesId->second),
                                 instancesCount2, size2));
      }
    }

    UpdateInfo();
  }


  OrthancPluginJobStepStatus TciaImportJob::Step()
  {
    if (position_ >= series_.size())
    {
      UpdateProgress(1);
      return OrthancPluginJobStepStatus_Success;
    }
    else
    {
      const Series& series = series_[position_];

      const std::string url = (std::string(TCIA_BASE_URL) +
                               "/getImage?SeriesInstanceUID=" + series.GetSeriesInstanceUid());

      Json::Value query;
      query["Level"] = "Instance";
      query["Query"]["SeriesInstanceUID"] = series.GetSeriesInstanceUid();
        
      Json::Value found;
      if (OrthancPlugins::RestApiPost(found, "/tools/find", query, false) &&
          found.type() == Json::arrayValue)
      {
        if (found.size() == series.GetInstancesCount())
        {
          LOG(INFO) << "TCIA series already fully stored in Orthanc: " << series.GetSeriesInstanceUid();
        }
        else
        {
          OrthancPlugins::MemoryBuffer buffer;
          if (buffer.HttpGet(url, "", ""))
          {
            std::string answer;
            if (!OrthancPlugins::RestApiPost(answer, "/instances", buffer.GetData(), buffer.GetSize(), false))
            {
              throw Orthanc::OrthancException(
                Orthanc::ErrorCode_BadFileFormat, "Cannot import series downloaded from TCIA into Orthanc: " +
                series.GetSeriesInstanceUid());
            }
          }
          else
          {
            throw Orthanc::OrthancException(
              Orthanc::ErrorCode_NetworkProtocol, "Cannot download series from TCIA: " +
              series.GetSeriesInstanceUid());
          }
        }
      }
      else
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }

      position_ ++;
        
      if (series_.size() > 0)
      {
        UpdateProgress(static_cast<float>(position_) / static_cast<float>(series_.size()));
      }

      return OrthancPluginJobStepStatus_Continue;
    }
  }


  std::string TciaImportJob::GetJobType()
  {
    return JOB_TYPE;
  }    
    

  TciaImportJob* TciaImportJob::Unserialize(const Json::Value& serialized)
  {
    if (!serialized.isMember(SERIES) ||
        serialized[SERIES].type() != Json::arrayValue)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat);
    }
    else
    {
      std::unique_ptr<TciaImportJob> job(new TciaImportJob);

      const Json::Value& series = serialized[SERIES];
      for (Json::Value::ArrayIndex i = 0; i < series.size(); i++)
      {
        job->AddSeriesInternal(Series::Unserialize(series[i]));
      }

      job->UpdateInfo();
        
      return job.release();
    }
  }
}
