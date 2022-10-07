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
#include <test/app/SetHook_wasm.h>

namespace ripple {

namespace test {


using TestHook = std::vector<uint8_t> const&;

class SetHook_test : public beast::unit_test::suite
{
public:

    void
    testHooksDisabled()
    {
        testcase("Check for disabled amendment");
        using namespace jtx;
        Env env{*this, supported_amendments() - featureHooks};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        // RH TODO: does it matter that passing malformed txn here gives back temMALFORMED (and not disabled)?
        env(ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0), ter(temDISABLED));
    }

//Json::Value
//hook(Account const& account, std::optional<std::vector<Json::Value>> hooks, std::uint32_t flags);

//Json::Value
//hso(std::vector<uint8_t> wasmBytes, uint64_t hookOn = 0, uint256 ns = beast::zero, uint8_t apiversion = 0);

    void
    testMalformedTxStructure()
    {
        testcase("Checks malformed transactions");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        // Must have a "Hooks" field
        env(ripple::test::jtx::hook(alice, {}, 0), ter(temMALFORMED));

        // Must have at least one non-empty subfield
        env(ripple::test::jtx::hook(alice, {{}}, 0), ter(temMALFORMED));

        // Must have fewer than 5 entries
        env(ripple::test::jtx::hook(alice, {{
                    hso(accept_wasm),
                    hso(accept_wasm),
                    hso(accept_wasm),
                    hso(accept_wasm),
                    hso(accept_wasm)}}, 0), ter(temMALFORMED));

        // Cannot have both CreateCode and HookHash
        {
            Json::Value jv = 
                ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0);
            Json::Value iv = jv[jss::Hooks][0U];
            iv[jss::Hook][jss::HookHash] = to_string(uint256{beast::zero});
            env(jv, ter(temMALFORMED));
        }

        // If createcode present must be less than 64kib
        env(ripple::test::jtx::hook(alice, {{hso(long_wasm)}}, 0), ter(temMALFORMED));
        
    }

    void
    testMalformedWasm()
    {
        testcase("Checks malformed hook binaries");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        // Must import guard
        env(ripple::test::jtx::hook(alice, {{hso(noguard_wasm)}}, 0), ter(temMALFORMED));

        // Must only contain hook and cbak
        env(ripple::test::jtx::hook(alice, {{hso(illegalfunc_wasm)}}, 0), ter(temMALFORMED));
    }

    void
    testAccept()
    {
        testcase("Test accept() hookapi");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        env(ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0), ter(tesSUCCESS));
    }
    
    void
    testRollback()
    {
        testcase("Test rollback() hookapi");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        env(ripple::test::jtx::hook(alice, {{hso(rollback_wasm)}}, 0), ter(tecHOOK_REJECTED));
    }

        // Trivial single hook
        //env(ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0));

        // RH TODO
    void
    run() override
    {
        //testTicketSetHook();  // RH TODO
        testHooksDisabled();
        testAccept();
        testRollback();
        testMalformedTxStructure();
        testMalformedWasm();
    }

private:
    TestHook
    accept_wasm =
    wasm[
        R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                return accept(0,0,0);
            }
        )[test.hook]"
    ];
    
    TestHook
    rollback_wasm =
    wasm[
        R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            #define SBUF(x) (uint32_t)(x),sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                return rollback(SBUF("Hook Rejected"),0);
            }
        )[test.hook]"
    ];
    
    TestHook
    noguard_wasm =
    wasm[
        R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            #define SBUF(x) (uint32_t)(x),sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                return accept(SBUF("Hook Accepted"),0);
            }
        )[test.hook]"
    ];

    TestHook
    illegalfunc_wasm =
    wasm[
        R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                return accept(0,0,0);
            }
            void otherfunc()
            {
                _g(1,1);
            }
        )[test.hook]"
    ];

    TestHook
    long_wasm =
    wasm[
        R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            #define M_REPEAT_10(X) X X X X X X X X X X
            #define M_REPEAT_100(X) M_REPEAT_10(M_REPEAT_10(X)) 
            #define M_REPEAT_1000(X) M_REPEAT_100(M_REPEAT_10(X)) 
            int64_t hook(uint32_t reserved )
            {
                char ret[] = M_REPEAT_1000("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890123");
                return accept(SBUF(ret), 0);
            }
        )[test.hook]"
    ];


};
BEAST_DEFINE_TESTSUITE(SetHook, tx, ripple);
}  // namespace test
}  // namespace ripple

/*
            Json::Value jv;
    
            jv[jss::Account] = alice.human();
            jv[jss::TransactionType] = jss::SetHook;
            jv[jss::Flags] = 0;
            jv[jss::Hooks] =
                Json::Value{Json::arrayValue};

            Json::Value iv;
                    
            iv[jss::CreateCode] = std::string(65536, 'F');
            iv[jss::HookOn] = "0000000000000000";
            iv[jss::HookNamespace] = to_string(uint256{beast::zero});
            iv[jss::HookApiVersion] = Json::Value{0};
            
            jv[jss::Hooks][i][jss::Hook] = iv;
            env(jv, ter(temMALFORMED));
*/
