/**
 * TCIA plugin for Orthanc
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "HttpCache.h"

#include <OrthancException.h>


static boost::posix_time::ptime GetNow()
{
  return boost::posix_time::second_clock::local_time();
}


namespace OrthancPlugins
{
  class HttpCache::Item : public boost::noncopyable
  {
  private:
    boost::posix_time::ptime  time_;
    std::string               body_;
    std::string               mime_;
      
  public:
    Item(const void* bodyData,
         size_t bodySize,
         const std::string& mime) :
      time_(GetNow()),
      body_(reinterpret_cast<const char*>(bodyData), bodySize),
      mime_(mime)
    {
    }

    bool HasExpired(const boost::posix_time::time_duration& duration) const
    {
      return (GetNow() - time_ >= duration);
    }

    const std::string& GetBody() const
    {
      return body_;
    }

    const std::string& GetMime() const
    {
      return mime_;
    }
  };
    

  HttpCache::HttpCache() :
    hasExpiration_(false),
    expiration_(boost::posix_time::hours(1))
  {
  }
    

  void HttpCache::SetExpiration(const boost::posix_time::time_duration& expiration)
  {
    if (expiration.total_milliseconds() <= 0)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }
    else
    {
      boost::mutex::scoped_lock lock(mutex_);
      hasExpiration_ = true;
      expiration_ = expiration;
    }
  }


  void HttpCache::ClearExpiration()
  {
    boost::mutex::scoped_lock lock(mutex_);
    hasExpiration_ = false;
  }

  
  void HttpCache::Clear()
  {
    boost::mutex::scoped_lock lock(mutex_);

    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }

    content_.clear();
  }

  
  bool HttpCache::Read(std::string& body,
                       std::string& mime,
                       const std::string& key)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Content::const_iterator found = content_.find(key);
    if (found == content_.end())
    {
      return false;
    }
    else
    {
      assert(found->second != NULL);
      if (hasExpiration_ &&
          found->second->HasExpired(expiration_))
      {
        return false;
      }
      else
      {
        body = found->second->GetBody();
        mime = found->second->GetMime();
        return true;
      }
    }
  }


  void HttpCache::Write(const std::string& key,
                        const void* bodyData,
                        size_t bodySize,
                        const std::string& mime)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Content::iterator found = content_.find(key);
    if (found == content_.end())
    {
      content_[key] = new Item(bodyData, bodySize, mime);
    }
    else
    {
      assert(found->second != NULL);
      delete found->second;

      found->second = new Item(bodyData, bodySize, mime);
    }
  }
    

  HttpCache& HttpCache::GetInstance()
  {
    static HttpCache cache;
    return cache;
  }
}
