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

package com.alibaba.graphar.graph

import com.alibaba.graphar.{GraphInfo, VertexInfo, EdgeInfo}
import com.alibaba.graphar.reader.{VertexReader, EdgeReader}

import org.apache.spark.sql.{DataFrame, SparkSession}

/**
 * The helper object for reading graph through the definitions of graph info.
 */
object GraphReader {

  /**
   * Loads the vertex chunks as DataFrame with the vertex infos.
   *
   * @param prefix
   *   The absolute prefix.
   * @param vertexInfos
   *   The map of (vertex label -> VertexInfo) for the graph.
   * @param spark
   *   The Spark session for the reading.
   * @return
   *   The map of (vertex label -> DataFrame)
   */
  private def readAllVertices(
      prefix: String,
      vertexInfos: Map[String, VertexInfo],
      spark: SparkSession
  ): Map[String, DataFrame] = {
    val vertex_dataframes: Map[String, DataFrame] = vertexInfos.map {
      case (label, vertexInfo) => {
        val reader = new VertexReader(prefix, vertexInfo, spark)
        (label, reader.readAllVertexPropertyGroups())
      }
    }
    return vertex_dataframes
  }

  /**
   * Loads the edge chunks as DataFrame with the edge infos.
   *
   * @param prefix
   *   The absolute prefix.
   * @param edgeInfos
   *   The map of ((srcLabel, edgeLabel, dstLabel) -> EdgeInfo) for the graph.
   * @param spark
   *   The Spark session for the reading.
   * @return
   *   The map of ((srcLabel, edgeLabel, dstLabel) -> (adj_list_type_str ->
   *   DataFrame))
   */
  private def readAllEdges(
      prefix: String,
      edgeInfos: Map[String, EdgeInfo],
      spark: SparkSession
  ): Map[(String, String, String), Map[String, DataFrame]] = {
    val edge_dataframes: Map[(String, String, String), Map[String, DataFrame]] =
      edgeInfos.map {
        case (key, edgeInfo) => {
          val adj_lists = edgeInfo.getAdj_lists
          val adj_list_it = adj_lists.iterator
          var adj_list_type_edge_df_map: Map[String, DataFrame] =
            Map[String, DataFrame]()
          while (adj_list_it.hasNext()) {
            val adj_list = adj_list_it.next()
            val adj_list_type = adj_list.getAdjList_type_in_gar
            val adj_list_type_str = adj_list.getAdjList_type
            val reader = new EdgeReader(prefix, edgeInfo, adj_list_type, spark)
            adj_list_type_edge_df_map += (adj_list_type_str -> reader.readEdges(
              false
            ))
          }
          (
            (
              edgeInfo.getSrc_label(),
              edgeInfo.getEdge_label(),
              edgeInfo.getDst_label()
            ),
            adj_list_type_edge_df_map
          )
        }
      }
    return edge_dataframes
  }

  /**
   * Reading the graph as vertex and edge DataFrames with the graph info object.
   *
   * @param graphInfo
   *   The info object for the graph.
   * @param spark
   *   The Spark session for the loading.
   * @return
   *   Pair of vertex DataFrames and edge DataFrames, the vertex DataFrames are
   *   stored as the map of (vertex_label -> DataFrame) the edge DataFrames are
   *   stored as a map of ((srcLabel, edgeLabel, dstLabel) -> (adj_list_type_str
   * -> DataFrame))
   */
  def readWithGraphInfo(
      graphInfo: GraphInfo,
      spark: SparkSession
  ): Pair[Map[String, DataFrame], Map[
    (String, String, String),
    Map[String, DataFrame]
  ]] = {
    val prefix = graphInfo.getPrefix
    val vertex_infos = graphInfo.getVertexInfos()
    val edge_infos = graphInfo.getEdgeInfos()
    return (
      readAllVertices(prefix, vertex_infos, spark),
      readAllEdges(prefix, edge_infos, spark)
    )
  }

  /**
   * Reading the graph as vertex and edge DataFrames with the graph info yaml
   * file.
   *
   * @param graphInfoPath
   *   The path of the graph info yaml.
   * @param spark
   *   The Spark session for the loading.
   * @return
   *   Pair of vertex DataFrames and edge DataFrames, the vertex DataFrames are
   *   stored as the map of (vertex_label -> DataFrame) the edge DataFrames are
   *   stored as a map of (srcLabel_edgeLabel_dstLabel -> (adj_list_type_str ->
   *   DataFrame))
   */
  def read(
      graphInfoPath: String,
      spark: SparkSession
  ): Pair[Map[String, DataFrame], Map[
    (String, String, String),
    Map[String, DataFrame]
  ]] = {
    // load graph info
    val graph_info = GraphInfo.loadGraphInfo(graphInfoPath, spark)

    // conduct reading
    readWithGraphInfo(graph_info, spark)
  }
}
