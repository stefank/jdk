/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_CPPSTDLIB_VECTOR_HPP
#define SHARE_CPPSTDLIB_VECTOR_HPP

#include "utilities/compilerWarnings.hpp"

#include "utilities/vmassert_uninstall.hpp"

PRAGMA_DIAG_PUSH
// warning C4530: C++ exception handler used, but unwind semantics are not
//                enabled. Specify /EHsc.
// Functions like std::vector<>::at() throw, but are never called
// by HotSpot code.
PRAGMA_DISABLE_MSVC_WARNING(4530)

// HotSpot usage:
// Must always use a HotSpot allocator.
// Must not use vector<>::at, because it may throw.
// Don't use std::vector<bool> - use BitMap instead.
//
// Convenience types: CHeapVector<T, MFLAGS> and ResourceAreaVector<T>.
#include <vector>

PRAGMA_DIAG_POP

#include "utilities/vmassert_reinstall.hpp"

#endif // SHARE_CPPSTDLIB_VECTOR_HPP
