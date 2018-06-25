/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Module:  n-test.c
**  Summary: native function test
**  Section: natives
**  Author:  Oldes
**
***********************************************************************/

#include "sys-core.h"

/**********************************************************************/
//
//  test-native: native [
//	  {This is just a native function for test purpose.}
//	  'value [any-type!] "This can be value of any type"
//  ]
//
REBNATIVE(test_native)
{
	REBVAL *val = D_ARG(1);
	printf("Value has type ID: %i\n", VAL_TYPE(val));
	return R_ARG1; //return the same value
}

