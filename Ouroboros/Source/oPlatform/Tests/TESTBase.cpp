// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#include <oBase/tests/oBaseTests.h>
#include "oTestIntegration.h"

using namespace ouro::tests;

#define oTEST_REGISTER_BASE_TEST0(_Name) oTEST_THROWS_REGISTER0(oCONCAT(oBase_, _Name), oCONCAT(TEST, _Name))
#define oTEST_REGISTER_BASE_TEST(_Name) oTEST_THROWS_REGISTER(oCONCAT(oBase_, _Name), oCONCAT(TEST, _Name))

#define oTEST_REGISTER_BASE_TEST_BUGGED0(_Name) oTEST_THROWS_REGISTER_BUGGED0(oCONCAT(oBase_, _Name), oCONCAT(TEST, _Name))
#define oTEST_REGISTER_BASE_TEST_BUGGED(_Name) oTEST_THROWS_REGISTER_BUGGED(oCONCAT(oBase_, _Name), oCONCAT(TEST, _Name))

oTEST_REGISTER_BASE_TEST0(aaboxf);
oTEST_REGISTER_BASE_TEST(compression);
oTEST_REGISTER_BASE_TEST0(concurrent_growable_object_pool);
oTEST_REGISTER_BASE_TEST(date);
oTEST_REGISTER_BASE_TEST0(equal);
oTEST_REGISTER_BASE_TEST0(filter_chain);
oTEST_REGISTER_BASE_TEST0(fourcc);
oTEST_REGISTER_BASE_TEST(hash_map);
oTEST_REGISTER_BASE_TEST0(osc);

