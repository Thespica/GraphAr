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

/*
 * Copyright 2021 Alibaba Group Holding Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.alibaba.graphar.stdcxx;

import static com.alibaba.graphar.util.CppClassName.ARROW_ARRAY;
import static com.alibaba.graphar.util.CppClassName.ARROW_TABLE;
import static com.alibaba.graphar.util.CppClassName.GAR_UTIL_INDEX_CONVERTER;
import static com.alibaba.graphar.util.CppHeaderName.ARROW_API_H;
import static com.alibaba.graphar.util.CppHeaderName.GAR_UTIL_UTIL_H;

import com.alibaba.fastffi.CXXHead;
import com.alibaba.fastffi.CXXTemplate;
import com.alibaba.fastffi.FFIGen;
import com.alibaba.fastffi.FFIPointer;
import com.alibaba.fastffi.FFITypeAlias;

@FFIGen
@CXXHead(system = "memory")
@CXXHead(ARROW_API_H)
@CXXHead(GAR_UTIL_UTIL_H)
@FFITypeAlias("std::shared_ptr")
@CXXTemplate(cxx = GAR_UTIL_INDEX_CONVERTER, java = "com.alibaba.graphar.util.IndexConverter")
@CXXTemplate(cxx = ARROW_TABLE, java = "com.alibaba.graphar.arrow.ArrowTable")
@CXXTemplate(cxx = ARROW_ARRAY, java = "com.alibaba.graphar.arrow.ArrowArray")
public interface StdSharedPtr<T extends FFIPointer> extends FFIPointer {
    // & will return the pointer of T.
    // shall be cxxvalue?
    T get();
}
