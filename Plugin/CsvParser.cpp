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


#include "CsvParser.h"

#include <OrthancException.h>

#include <cassert>
#include <csv.h>


static int IsSpace(unsigned char c)
{
  return (c == CSV_SPACE || c == CSV_TAB) ? 1 : 0;
}


static int IsEndOfLine(unsigned char c)
{
  return (c == CSV_CR || c == CSV_LF) ? 1 : 0;
}


namespace OrthancPlugins
{
  void CsvParser::OnCell(void *content,
                         size_t size,
                         void *payload)
  {
    CsvParser& that = *reinterpret_cast<CsvParser*>(payload);

    std::string s(reinterpret_cast<const char*>(content), size);
    that.cells_.push_back(s);
  }

  
  void CsvParser::OnNextRow(int c,
                            void *payload)
  {
    CsvParser& that = *reinterpret_cast<CsvParser*>(payload);
      
    that.startOfRowIndex_.push_back(that.cells_.size());
  }


  void CsvParser::Parse(const std::string& csv)
  {
    cells_.clear();
    startOfRowIndex_.clear();
    startOfRowIndex_.push_back(0);

    if (!csv.empty())
    {
      struct csv_parser p;

      if (csv_init(&p, 0) != 0)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError, "Failed to initialize CSV parser");
      }
      
      csv_set_space_func(&p, IsSpace);
      csv_set_term_func(&p, IsEndOfLine);

      bool ok = (csv_parse(&p, csv.c_str(), csv.size(), OnCell, OnNextRow, this) == csv.size() &&
                 csv_fini(&p, OnCell, OnNextRow, this) == 0);

      csv_free(&p);

      if (!ok)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "Cannot parse CSV");
      }
    }
  }

  
  size_t CsvParser::GetRowsCount() const
  {
    assert(startOfRowIndex_.size() > 0);
    return (startOfRowIndex_.size() - 1);
  }

  
  size_t CsvParser::GetColumnsCount(size_t row) const
  {
    if (row >= GetRowsCount())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }
    else
    {
      assert(row + 1 < startOfRowIndex_.size());
      return (startOfRowIndex_[row + 1] - startOfRowIndex_[row]);
    }
  }

  
  const std::string& CsvParser::GetCell(size_t row,
                                        size_t column) const
  {
    if (column >= GetColumnsCount(row))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return cells_[startOfRowIndex_[row] + column];
    }
  }

  
  void CsvParser::GetHeaderIndex(std::map<std::string, size_t>& index) const
  {
    index.clear();

    if (GetRowsCount() > 0)
    {
      const size_t n = GetColumnsCount(0);
      assert(cells_.size() >= n);

      for (size_t i = 0; i < n; i++)
      {
        const std::string& s = cells_[i];

        if (index.find(s) != index.end())
        {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "The header row is not a valid index");
        }
        else
        {
          index[s] = i;
        }
      }
    }
  }
}
