/*
 * Copyright 2022-2023 Alibaba Group Holding Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdlib>

#include "arrow/api.h"

#include "./util.h"
#include "gar/reader/arrow_chunk_reader.h"
#include "gar/util/adj_list_type.h"
#include "gar/util/expression.h"
#include "gar/util/general_params.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

using GAR_NAMESPACE::_And;
using GAR_NAMESPACE::_Equal;
using GAR_NAMESPACE::_LessThan;
using GAR_NAMESPACE::_Literal;
using GAR_NAMESPACE::_Property;
using GAR_NAMESPACE::util::FilterOptions;

namespace GAR_NAMESPACE {

TEST_CASE("ArrowChunkReader") {
  std::string root;
  REQUIRE(GetTestResourceRoot(&root).ok());

  // read file and construct graph info
  std::string path = root + "/ldbc_sample/parquet/ldbc_sample.graph.yml";
  std::string src_label = "person", edge_label = "knows", dst_label = "person";
  std::string vertex_property_name = "id";
  std::string edge_property_name = "creationDate";
  auto maybe_graph_info = GraphInfo::Load(path);
  REQUIRE(maybe_graph_info.status().ok());
  auto graph_info = maybe_graph_info.value();
  auto vertex_info = graph_info->GetVertexInfo(src_label);
  REQUIRE(vertex_info != nullptr);
  auto v_pg = vertex_info->GetPropertyGroup(vertex_property_name);
  REQUIRE(v_pg != nullptr);
  auto edge_info = graph_info->GetEdgeInfo(src_label, edge_label, dst_label);
  REQUIRE(edge_info != nullptr);
  auto e_pg = edge_info->GetPropertyGroup(edge_property_name);
  REQUIRE(e_pg != nullptr);

  SECTION("VertexPropertyArrowChunkReader") {
    auto maybe_reader = VertexPropertyArrowChunkReader::Make(
        graph_info, src_label, vertex_property_name);
    REQUIRE(maybe_reader.status().ok());
    auto reader = maybe_reader.value();
    REQUIRE(reader->GetChunkNum() == 10);

    SECTION("Basics") {
      auto result = reader->GetChunk();
      REQUIRE(!result.has_error());
      auto table = result.value();
      REQUIRE(table->num_rows() == 100);
      REQUIRE(table->GetColumnByName(GeneralParams::kVertexIndexCol) !=
              nullptr);

      // seek
      REQUIRE(reader->seek(100).ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 100);
      REQUIRE(table->GetColumnByName(GeneralParams::kVertexIndexCol) !=
              nullptr);
      REQUIRE(reader->next_chunk().ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 100);
      REQUIRE(table->GetColumnByName(GeneralParams::kVertexIndexCol) !=
              nullptr);
      REQUIRE(reader->seek(900).ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 3);
      REQUIRE(table->GetColumnByName(GeneralParams::kVertexIndexCol) !=
              nullptr);
      REQUIRE(reader->GetChunkNum() == 10);
      REQUIRE(reader->next_chunk().IsIndexError());

      REQUIRE(reader->seek(1024).IsIndexError());
    }

    SECTION("PropertyPushDown") {
      std::string filter_property = "gender";
      auto filter = _Equal(_Property(filter_property), _Literal("female"));
      std::vector<std::string> expected_cols{"firstName", "lastName"};
      // print reader result
      auto walkReader =
          [&](std::shared_ptr<VertexPropertyArrowChunkReader>& reader) {
            int idx = 0, sum = 0;
            std::shared_ptr<arrow::Table> table;

            do {
              auto result = reader->GetChunk();
              REQUIRE(!result.has_error());
              table = result.value();
              std::cout << "Chunk: " << idx << ",\tNums: " << table->num_rows()
                        << '\n';
              idx++;
              sum += table->num_rows();
            } while (!reader->next_chunk().IsIndexError());
            REQUIRE(idx == reader->GetChunkNum());
            REQUIRE(table->num_columns() == (int) expected_cols.size());

            std::cout << "Total Nums: " << sum << "/"
                      << reader->GetChunkNum() * vertex_info->GetChunkSize()
                      << '\n';
            std::cout << "Column Nums: " << table->num_columns() << "\n";
            std::cout << "Column Names: ";
            for (int i = 0; i < table->num_columns(); i++) {
              REQUIRE(table->ColumnNames()[i] == expected_cols[i]);
              std::cout << "`" << table->ColumnNames()[i] << "` ";
            }
            std::cout << "\n\n";
          };

      SECTION("pushdown by helper function") {
        std::cout << "Vertex property pushdown by helper function:\n";
        // construct push down options
        FilterOptions options;
        options.filter = filter;
        options.columns = expected_cols;
        auto maybe_reader = VertexPropertyArrowChunkReader::Make(
            graph_info, src_label, filter_property, options);
        REQUIRE(maybe_reader.status().ok());
        walkReader(maybe_reader.value());
      }

      SECTION("pushdown by function Filter() & Select()") {
        std::cout << "Vertex property pushdown by Filter() & Select():\n";
        auto maybe_reader = VertexPropertyArrowChunkReader::Make(
            graph_info, src_label, filter_property);
        REQUIRE(maybe_reader.status().ok());
        auto reader = maybe_reader.value();
        reader->Filter(filter);
        reader->Select(expected_cols);
        walkReader(reader);
      }

      SECTION("pushdown property that don't exist") {
        std::cout << "Vertex property pushdown property that don't exist:\n";
        auto filter = _Equal(_Property("id"), _Literal(933));
        FilterOptions options;
        options.filter = filter;
        options.columns = expected_cols;
        auto maybe_reader = VertexPropertyArrowChunkReader::Make(
            graph_info, src_label, filter_property, options);
        REQUIRE(maybe_reader.status().ok());
        auto reader = maybe_reader.value();
        auto result = reader->GetChunk();
        REQUIRE(result.error().IsInvalid());
        std::cerr << result.error().message() << std::endl;
      }

      SECTION("pushdown column that don't exist") {
        std::cout << "Vertex property pushdown column that don't exist:\n";
        auto filter = _Literal(true);
        std::vector<std::string> expected_cols{"id"};
        FilterOptions options;
        options.filter = filter;
        options.columns = expected_cols;
        auto maybe_reader = VertexPropertyArrowChunkReader::Make(
            graph_info, src_label, filter_property, options);
        REQUIRE(maybe_reader.status().ok());
        auto reader = maybe_reader.value();
        auto result = reader->GetChunk();
        REQUIRE(result.error().IsInvalid());
        std::cerr << result.error().message() << std::endl;
      }
    }

    SECTION("Make from graph info and property group") {
      auto maybe_reader =
          VertexPropertyArrowChunkReader::Make(graph_info, src_label, v_pg);
      REQUIRE(maybe_reader.status().ok());
      auto reader = maybe_reader.value();
      REQUIRE(reader->GetChunkNum() == 10);
    }

    SECTION("Make from vertex info and property group") {
      auto maybe_reader = VertexPropertyArrowChunkReader::Make(
          vertex_info, v_pg, graph_info->GetPrefix());
      REQUIRE(maybe_reader.status().ok());
      auto reader = maybe_reader.value();
      REQUIRE(reader->GetChunkNum() == 10);
    }
  }

  SECTION("AdjListArrowChunkReader") {
    auto maybe_reader = AdjListArrowChunkReader::Make(
        graph_info, src_label, edge_label, dst_label,
        AdjListType::ordered_by_source);
    REQUIRE(maybe_reader.status().ok());
    auto reader = maybe_reader.value();
    SECTION("Basics") {
      auto result = reader->GetChunk();
      REQUIRE(!result.has_error());
      auto table = result.value();
      REQUIRE(table->num_rows() == 667);

      // seek
      REQUIRE(reader->seek(100).ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 567);
      REQUIRE(reader->GetRowNumOfChunk() == 667);
      REQUIRE(reader->next_chunk().ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 644);
      REQUIRE(reader->seek(1024).IsIndexError());

      // seek src & dst
      REQUIRE(reader->seek_src(100).ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 644);
      REQUIRE(!reader->seek_dst(100).ok());

      REQUIRE(reader->seek_src(900).ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 4);

      REQUIRE(reader->next_chunk().IsIndexError());
    }

    SECTION("Make from edge info") {
      auto maybe_reader = AdjListArrowChunkReader::Make(
          edge_info, AdjListType::ordered_by_source, graph_info->GetPrefix());
      REQUIRE(maybe_reader.status().ok());
    }

    SECTION("set start vertex chunk index by seek_chunk_index") {
      auto maybe_reader = AdjListArrowChunkReader::Make(
          graph_info, src_label, edge_label, dst_label,
          AdjListType::ordered_by_source);
      auto reader = maybe_reader.value();
      // check reader start from vertex chunk 0
      auto result = reader->GetChunk();
      REQUIRE(!result.has_error());
      auto table = result.value();
      REQUIRE(table->num_rows() == 667);
      // set start vertex chunk index to 1
      reader->seek_chunk_index(1);
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 644);
    }
  }

  SECTION("AdjListPropertyArrowChunkReader") {
    auto maybe_reader = AdjListPropertyArrowChunkReader::Make(
        graph_info, src_label, edge_label, dst_label, edge_property_name,
        GAR_NAMESPACE::AdjListType::ordered_by_source);
    REQUIRE(maybe_reader.status().ok());
    auto reader = maybe_reader.value();

    SECTION("Basics") {
      auto result = reader->GetChunk();
      REQUIRE(!result.has_error());
      auto table = result.value();
      REQUIRE(table->num_rows() == 667);

      // seek
      REQUIRE(reader->seek(100).ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 567);
      REQUIRE(reader->next_chunk().ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 644);
      REQUIRE(reader->seek(1024).IsIndexError());

      // seek src & dst
      REQUIRE(reader->seek_src(100).ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 644);
      REQUIRE(!reader->seek_dst(100).ok());

      REQUIRE(reader->seek_src(900).ok());
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 4);

      REQUIRE(reader->next_chunk().IsIndexError());
    }

    SECTION("PropertyPushDown") {
      // construct pushdown options
      auto expr1 = _LessThan(_Literal("2012-06-02T04:30:44.526+0000"),
                             _Property(edge_property_name));
      auto expr2 =
          _Equal(_Property(edge_property_name), _Property(edge_property_name));
      auto filter = _And(expr1, expr2);

      std::vector<std::string> expected_cols{"creationDate"};

      FilterOptions options;
      options.filter = filter;
      options.columns = expected_cols;

      // print reader result
      auto walkReader =
          [&](std::shared_ptr<AdjListPropertyArrowChunkReader>& reader) {
            int idx = 0, sum = 0;
            std::shared_ptr<arrow::Table> table;

            do {
              auto result = reader->GetChunk();
              REQUIRE(!result.has_error());
              table = result.value();
              std::cout << "Chunk: " << idx << ",\tNums: " << table->num_rows()
                        << "/" << edge_info->GetChunkSize() << '\n';
              idx++;
              sum += table->num_rows();
            } while (!reader->next_chunk().IsIndexError());
            REQUIRE(table->num_columns() == (int) expected_cols.size());

            std::cout << "Total Nums: " << sum << "/"
                      << idx * edge_info->GetChunkSize() << '\n';
            std::cout << "Column Nums: " << table->num_columns() << "\n";
            std::cout << "Column Names: ";
            for (int i = 0; i < table->num_columns(); i++) {
              REQUIRE(table->ColumnNames()[i] == expected_cols[i]);
              std::cout << "`" << table->ColumnNames()[i] << "` ";
            }
            std::cout << "\n\n";
          };

      SECTION("pushdown by helper function") {
        std::cout << "Adj list property pushdown by helper function: \n";
        auto maybe_reader = AdjListPropertyArrowChunkReader::Make(
            graph_info, src_label, edge_label, dst_label, edge_property_name,
            GAR_NAMESPACE::AdjListType::ordered_by_source, options);
        REQUIRE(maybe_reader.status().ok());
        auto reader = maybe_reader.value();
        walkReader(reader);
      }

      SECTION("pushdown by function Filter() & Select()") {
        std::cout << "Adj list property pushdown by Filter() & Select():"
                  << std::endl;
        auto maybe_reader = AdjListPropertyArrowChunkReader::Make(
            graph_info, src_label, edge_label, dst_label, edge_property_name,
            GAR_NAMESPACE::AdjListType::ordered_by_source);
        REQUIRE(maybe_reader.status().ok());
        auto reader = maybe_reader.value();
        reader->Filter(filter);
        reader->Select(expected_cols);
        walkReader(reader);
      }
    }

    SECTION("set start vertex chunk index by seek_chunk_index") {
      auto maybe_reader = AdjListPropertyArrowChunkReader::Make(
          graph_info, src_label, edge_label, dst_label, edge_property_name,
          GAR_NAMESPACE::AdjListType::ordered_by_source);
      REQUIRE(maybe_reader.status().ok());
      auto reader = maybe_reader.value();
      // check reader start from vertex chunk 0
      auto result = reader->GetChunk();
      REQUIRE(!result.has_error());
      auto table = result.value();
      REQUIRE(table->num_rows() == 667);
      // set start vertex chunk index to 1
      reader->seek_chunk_index(1);
      result = reader->GetChunk();
      REQUIRE(!result.has_error());
      table = result.value();
      REQUIRE(table->num_rows() == 644);
    }
  }

  SECTION("AdjListOffsetArrowChunkReader") {
    auto maybe_reader = AdjListOffsetArrowChunkReader::Make(
        graph_info, src_label, edge_label, dst_label,
        GAR_NAMESPACE::AdjListType::ordered_by_source);
    REQUIRE(maybe_reader.status().ok());
    auto reader = maybe_reader.value();
    auto result = reader->GetChunk();
    REQUIRE(!result.has_error());
    auto array = result.value();
    REQUIRE(array->length() == 101);
    REQUIRE(reader->next_chunk().ok());
    result = reader->GetChunk();
    REQUIRE(!result.has_error());
    array = result.value();
    REQUIRE(array->length() == 101);

    // seek
    REQUIRE(reader->seek(900).ok());
    result = reader->GetChunk();
    REQUIRE(!result.has_error());
    array = result.value();
    REQUIRE(array->length() == 4);
    REQUIRE(reader->next_chunk().IsIndexError());
    REQUIRE(reader->seek(1024).IsIndexError());
  }
}
}  // namespace GAR_NAMESPACE
