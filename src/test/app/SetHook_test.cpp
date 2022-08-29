//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2016 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/jss.h>
#include <test/jtx.h>
#include <test/jtx/hook.h>

namespace ripple {

namespace test {

class SetHook_test : public beast::unit_test::suite
{
private:
    std::vector<uint8_t> const
    accept_wasm = 
    {
        /*
        (module
          (type (;0;) (func (param i32 i32 i64) (result i64)))
          (type (;1;) (func (param i32 i32) (result i32)))
          (type (;2;) (func (param i32) (result i64)))
          (import "env" "accept" (func (;0;) (type 0)))
          (import "env" "_g" (func (;1;) (type 1)))
          (func (;2;) (type 2) (param i32) (result i64)
            (local i32)
            i32.const 0
            i32.const 0
            i64.const 0
            call 0
            drop
            i32.const 1
            i32.const 1
            call 1
            drop
            i64.const 0)
          (memory (;0;) 2)
          (export "hook" (func 2)))
        */  
        0x00U,0x61U,0x73U,0x6dU,0x01U,0x00U,0x00U,0x00U,0x01U,0x13U,0x03U,0x60U,0x03U,0x7fU,0x7fU,0x7eU,0x01U,0x7eU,
        0x60U,0x02U,0x7fU,0x7fU,0x01U,0x7fU,0x60U,0x01U,0x7fU,0x01U,0x7eU,0x02U,0x17U,0x02U,0x03U,0x65U,0x6eU,0x76U,
        0x06U,0x61U,0x63U,0x63U,0x65U,0x70U,0x74U,0x00U,0x00U,0x03U,0x65U,0x6eU,0x76U,0x02U,0x5fU,0x67U,0x00U,0x01U,
        0x03U,0x02U,0x01U,0x02U,0x05U,0x03U,0x01U,0x00U,0x02U,0x07U,0x08U,0x01U,0x04U,0x68U,0x6fU,0x6fU,0x6bU,0x00U,
        0x02U,0x0aU,0x18U,0x01U,0x16U,0x01U,0x01U,0x7fU,0x41U,0x00U,0x41U,0x00U,0x42U,0x00U,0x10U,0x00U,0x1aU,0x41U,
        0x01U,0x41U,0x01U,0x10U,0x01U,0x1aU,0x42U,0x00U,0x0bU
    };

public:


    void
    testHooksDisabled()
    {
        testcase("SetHook checks for disabled amendment");
        using namespace jtx;
        Env env{*this, supported_amendments() - featureHooks};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env(ripple::test::jtx::hook(alice, {}, 0), ter(temDISABLED));
    }

    void
    testMalformedTransaction()
    {
        testcase("SetHook checks for malformed transactions");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);


        // Must have a "Hooks" field
        env(ripple::test::jtx::hook(alice, {}, 0), ter(temMALFORMED));

        // Must have at least one non-empty subfield 
        env(ripple::test::jtx::hook(alice, {{}}, 0), ter(temMALFORMED));

        // Trivial single hook
        env(ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0));

        // RH TODO
    }

    void
    run() override
    {
        //testTicketSetHook();  // RH TODO
        testHooksDisabled();
        testMalformedTransaction();
    }
};
BEAST_DEFINE_TESTSUITE(SetHook, tx, ripple);
}  // namespace test
}  // namespace ripple
