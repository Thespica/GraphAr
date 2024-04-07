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

import org.apache.graphar.TestTools;
import org.apache.graphar.info.type.DataType;
import org.apache.graphar.info.yaml.PropertyYamlParser;
import org.junit.Assert;
import org.junit.Test;

public class PropertyTest {
    @Test
    public void testLoadProperty() {
        final String propertyContent =
                        "name: id\n" +
                        "data_type: int64\n" +
                        "is_primary: true\n" +
                        "is_nullable: false";
        final PropertyYamlParser propertyYamlParser = TestTools.loadYaml(propertyContent, PropertyYamlParser.class);
        final Property property = new Property(propertyYamlParser);
        Assert.assertEquals(property.getName(), "id");
        Assert.assertEquals(property.getDataType(), DataType.INT64);
        Assert.assertTrue(property.isPrimary());
        Assert.assertFalse(property.isNullable());
    }

    @Test
    public void testLoadPropertyWithoutNullable() {
        // when is_nullable is not provided, it's value should be !is_primary
        // test property is primary
        final String propertyContentIsPrimary =
                        "name: id\n" +
                        "data_type: int64\n" +
                        "is_primary: true";
        final PropertyYamlParser propertyYamlParserIsPrimary = TestTools.loadYaml(propertyContentIsPrimary, PropertyYamlParser.class);
        final Property propertyIsPrimary = new Property(propertyYamlParserIsPrimary);
        Assert.assertFalse(propertyIsPrimary.isNullable());
        // test property is not primary
        final String propertyContentIsNotPrimary =
                        "name: id\n" +
                        "data_type: int64\n" +
                        "is_primary: false";
        final PropertyYamlParser propertyYamlParserIsNotPrimary = TestTools.loadYaml(propertyContentIsNotPrimary, PropertyYamlParser.class);
        final Property propertyIsNotPrimary = new Property(propertyYamlParserIsNotPrimary);
        Assert.assertTrue(propertyIsNotPrimary.isNullable());
    }
}
