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

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <map>


namespace OrthancPlugins
{
  class HttpCache : public boost::noncopyable
  {
  private:
    class Item;
    
    typedef std::map<std::string, Item*>  Content;
    
    boost::mutex  mutex_;
    Content       content_;
    bool          hasExpiration_;
    boost::posix_time::time_duration  expiration_;

  public:
    HttpCache();
    
    ~HttpCache()
    {
      Clear();
    }

    void SetExpiration(const boost::posix_time::time_duration& expiration);

    void ClearExpiration();
    
    void Clear();

    bool Read(std::string& body,
              std::string& mime,
              const std::string& key);
    
    void Write(const std::string& key,
               const void* bodyData,
               size_t bodySize,
               const std::string& mime);
    
    static HttpCache& GetInstance();
  };
}
