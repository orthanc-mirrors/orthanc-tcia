/**
 * TCIA plugin for Orthanc
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <boost/noncopyable.hpp>
#include <deque>
#include <map>
#include <string>


namespace OrthancPlugins
{
  class CsvParser : public boost::noncopyable
  {
  private:
    std::deque<std::string>  cells_;
    std::deque<size_t>       startOfRowIndex_;

    static void OnCell(void *content,
                       size_t size,
                       void *payload);

    static void OnNextRow(int c,
                          void *payload);

  public:
    void Parse(const std::string& csv);

    size_t GetRowsCount() const;

    size_t GetColumnsCount(size_t row) const;

    const std::string& GetCell(size_t row,
                               size_t column) const;

    void GetHeaderIndex(std::map<std::string, size_t>& index) const;
  };
}
