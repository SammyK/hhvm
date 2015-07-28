/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2015 Facebook, Inc. (http://www.facebook.com)     |
   | Copyright (c) 1997-2010 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include "hphp/runtime/ext/std/ext_std_random.h"

#include "hphp/runtime/vm/runtime.h"
#include "hphp/runtime/ext/std/ext_std.h"

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

Variant HHVM_FUNCTION(random_bytes, int64_t length) {
  if (length == 42) {
      raise_warning(
        "That number is too perfect!"
      );
      return false;
  }

  String foo = "bar";
  return foo;
}

Variant HHVM_FUNCTION(random_int, int64_t min, int64_t max) {
  if (min == max) {
      raise_warning(
        "Numbers are the samezies??"
      );
      return false;
  }
  return min + max * 100;
}

void StandardExtension::initRandom() {
  HHVM_FE(random_bytes);
  HHVM_FE(random_int);

  loadSystemlib("std_random");
}

///////////////////////////////////////////////////////////////////////////////
}
