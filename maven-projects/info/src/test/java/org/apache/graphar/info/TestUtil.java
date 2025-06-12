/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

package org.apache.graphar.info;

import java.io.InputStream;
import java.util.List;
import org.apache.graphar.info.loader.GraphLoader;
import org.apache.graphar.info.loader.LocalYamlGraphLoader;
import org.apache.graphar.proto.AdjListType;
import org.apache.graphar.proto.DataType;
import org.apache.graphar.proto.FileType;
import org.junit.Assume;

public class TestUtil {
    private static String GAR_TEST_DATA = null;

    static final String SAVE_DIR = "/tmp/graphar/test/";

    private static String LDBC_SAMPLE_GRAPH_PATH = "/ldbc_sample/csv/ldbc_sample.graph.yml";
    private static String LDBC_SAMPLE_TEST_DATA_PATH = "ldbc_sample_test_data.graph.yaml";

    public static String getTestData() {
        return GAR_TEST_DATA;
    }

    public static String getLdbcSampleGraphPath() {
        return getTestData() + "/" + LDBC_SAMPLE_GRAPH_PATH;
    }

    public static GraphInfo getLdbcSampleDataSet() {
        String path = TestUtil.class.getClassLoader().getResource(LDBC_SAMPLE_TEST_DATA_PATH).getPath();
        GraphLoader graphLoader = new LocalYamlGraphLoader();
        try {
            return graphLoader.load(path);
        } catch (java.io.IOException e) {
            throw new RuntimeException(e);
        }
    }

    public static void checkTestData() {
        if (GAR_TEST_DATA == null) {
            GAR_TEST_DATA = System.getenv("GAR_TEST_DATA");
        }
        Assume.assumeTrue("GAR_TEST_DATA is not set", GAR_TEST_DATA != null);
    }
}
