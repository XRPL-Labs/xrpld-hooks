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
#include <ripple/app/tx/impl/SetHook.h>
#include <unordered_map>

namespace ripple {

namespace test {

#define DEBUG_TESTS 1

using TestHook = std::vector<uint8_t> const&;

class JSSHasher
{
public:
    size_t operator()(const Json::StaticString& n) const
    {
        return std::hash<std::string_view>{}(n.c_str());
    }
};

class JSSEq
{
public:
    bool operator()(const Json::StaticString& a, const Json::StaticString& b) const
    {
        return a == b;
    }
};

using JSSMap = std::unordered_map<Json::StaticString, Json::Value, JSSHasher, JSSEq>;

// Identical to BEAST_EXPECT except it returns from the function
// if the condition isn't met (and would otherwise therefore cause a crash)
#define BEAST_REQUIRE(x)\
{\
    BEAST_EXPECT(!!(x));\
    if (!(x))\
        return;\
}


class SetHook_test : public beast::unit_test::suite
{
private:
    // helper
    void static overrideFlag(Json::Value& jv)
    {
        jv[jss::Flags] = hsfOVERRIDE;
    }

public:

    // This is a large fee, large enough that we can set most small test hooks without running into fee issues
    // we only want to test fee code specifically in fee unit tests, the rest of the time we want to ignore it.
    #define HSFEE fee(10'000'000)
    #define M(m) memo(m, "", "")
    void
    testHooksDisabled()
    {
        testcase("Check for disabled amendment");
        using namespace jtx;
        Env env{*this, supported_amendments() - featureHooks};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        // RH TODO: does it matter that passing malformed txn here gives back temMALFORMED (and not disabled)?
        env(ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0),
            M("Hooks Disabled"),
            HSFEE, ter(temDISABLED));
    }

    void
    testTxStructure()
    {
        testcase("Checks malformed transactions");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.close();

        // Test outer structure

        env(ripple::test::jtx::hook(alice, {}, 0),
            M("Must have a hooks field"),
            HSFEE, ter(temMALFORMED));


        env(ripple::test::jtx::hook(alice, {{}}, 0),
            M("Must have a non-empty hooks field"),
            HSFEE, ter(temMALFORMED));

        env(ripple::test::jtx::hook(alice, {{
                hso(accept_wasm),
                hso(accept_wasm),
                hso(accept_wasm),
                hso(accept_wasm),
                hso(accept_wasm)}}, 0),
            M("Must have fewer than 5 entries"),
            HSFEE, ter(temMALFORMED));

        {
            Json::Value jv;
            jv[jss::Account] = alice.human();
            jv[jss::TransactionType] = jss::SetHook;
            jv[jss::Flags] = 0;
            jv[jss::Hooks] = Json::Value{Json::arrayValue};

            Json::Value iv;
            iv[jss::MemoData] = "DEADBEEF";
            iv[jss::MemoFormat] = "";
            iv[jss::MemoType] = "";
            jv[jss::Hooks][0U][jss::Memo] = iv;
            env(jv,
                M("Hooks Array must contain Hook objects"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }



    }

    void testGrants()
    {
        testcase("Checks malformed grants on install operation");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        // check too many grants
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value grants {Json::arrayValue};
            for (uint32_t i = 0; i < 9; ++i)
            {
                Json::Value pv;
                Json::Value piv;
                piv[jss::HookHash] = to_string(uint256{i});
                pv[jss::HookGrant] = piv;
                grants[i] = pv;
            }
            iv[jss::HookGrants] = grants;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must not include more than 8 grants"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // check wrong inner type
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value grants {Json::arrayValue};
            grants[0U] = Json::Value{};
            grants[0U][jss::Memo] = Json::Value{};
            grants[0U][jss::Memo][jss::MemoFormat] = strHex(std::string(12, 'a'));
            grants[0U][jss::Memo][jss::MemoData] = strHex(std::string(12, 'a'));
            iv[jss::HookGrants] = grants;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO grant array can only contain HookGrant objects"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

    }

    void testParams()
    {
        testcase("Checks malformed params on install operation");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        // check too many parameters
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params {Json::arrayValue};
            for (uint32_t i = 0; i < 17; ++i)
            {
                Json::Value pv;
                Json::Value piv;
                piv[jss::HookParameterName] = strHex("param" + std::to_string(i));
                piv[jss::HookParameterValue] = strHex("value" + std::to_string(i));
                pv[jss::HookParameter] = piv;
                params[i] = pv;
            }
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must not include more than 16 parameters"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // check repeat parameters
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params {Json::arrayValue};
            for (uint32_t i = 0; i < 2; ++i)
            {
                params[i] = Json::Value{};
                params[i][jss::HookParameter] = Json::Value{};
                params[i][jss::HookParameter][jss::HookParameterName] = strHex(std::string{"param"});
            }
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must not repeat parameter names"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // check too long parameter name
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params {Json::arrayValue};
            params[0U] = Json::Value{};
            params[0U][jss::HookParameter] = Json::Value{};
            params[0U][jss::HookParameter][jss::HookParameterName] = strHex(std::string(33, 'a'));
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must must not contain parameter names longer than 32 bytes"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // check too long parameter value
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params {Json::arrayValue};
            params[0U] = Json::Value{};
            params[0U][jss::HookParameter] = Json::Value{};
            params[0U][jss::HookParameter][jss::HookParameterName] = strHex(std::string(32, 'a'));
            params[0U][jss::HookParameter][jss::HookParameterValue] = strHex(std::string(129, 'a'));
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must must not contain parameter values longer than 128 bytes"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // wrong object type
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params {Json::arrayValue};
            params[0U] = Json::Value{};
            params[0U][jss::Memo] = Json::Value{};
            params[0U][jss::Memo][jss::MemoFormat] = strHex(std::string(12, 'a'));
            params[0U][jss::Memo][jss::MemoData] = strHex(std::string(12, 'a'));
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO parameter array can only contain HookParameter objects"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

    }

    void testInstall()
    {
        testcase("Checks malformed install operation");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        auto const bob = Account{"bob"};
        env.fund(XRP(10000), bob);

        // create a hook that we can then install
        {
            env(ripple::test::jtx::hook(bob, {{
                hso(accept_wasm),
                hso(rollback_wasm)
            }}, 0),
                M("First set = tesSUCCESS"),
                HSFEE, ter(tesSUCCESS));
        }

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        // can't set api version
        {
            Json::Value iv;
            iv[jss::HookHash] = accept_hash_str;
            iv[jss::HookApiVersion] = 1U;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation cannot set apiversion"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // can't set non-existent hook
        {
            Json::Value iv;
            iv[jss::HookHash] = "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF";
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation cannot set non existent hook hash"),
                HSFEE, ter(terNO_HOOK));
            env.close();
        }

        // can set extant hook
        {
            Json::Value iv;
            iv[jss::HookHash] = accept_hash_str;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation can set extant hook hash"),
                HSFEE, ter(tesSUCCESS));
            env.close();
        }

        // can't set extant hook over other hook without override flag
        {
            Json::Value iv;
            iv[jss::HookHash] = rollback_hash_str;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation can set extant hook hash"),
                HSFEE, ter(tecREQUIRES_FLAG));
            env.close();
        }

        // can set extant hook over other hook with override flag
        {
            Json::Value iv;
            iv[jss::HookHash] = rollback_hash_str;
            iv[jss::Flags] = hsfOVERRIDE;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation can set extant hook hash"),
                HSFEE, ter(tesSUCCESS));
            env.close();
        }
    }

    void testDelete()
    {
        testcase("Checks malformed delete operation");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        // flag required
        {
            Json::Value iv;
            iv[jss::CreateCode] = "";
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook DELETE operation must include hsfOVERRIDE flag"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // invalid flags
        {
            Json::Value iv;
            iv[jss::CreateCode] = "";
            iv[jss::Flags] = "2147483648";
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook DELETE operation must include hsfOVERRIDE flag"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // grants, parameters, hookon, hookapiversion, hooknamespace keys must be absent
        for (auto const& [key, value]:
            JSSMap {
                {jss::HookGrants, Json::arrayValue},
                {jss::HookParameters, Json::arrayValue},
                {jss::HookOn, "0"},
                {jss::HookApiVersion, "0"},
                {jss::HookNamespace, to_string(uint256{beast::zero})}
            })
        {
            Json::Value iv;
            iv[jss::CreateCode] = "";
            iv[key] = value;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook DELETE operation cannot include: grants, params, hookon, apiversion, namespace"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // create and delete single hook
        {
            {
                Json::Value jv =
                    ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0);
                env(jv,
                    M("Normal accept create"),
                    HSFEE, ter(tesSUCCESS));
                env.close();
            }

            BEAST_REQUIRE(env.le(accept_keylet));

            Json::Value iv;
            iv[jss::CreateCode] = "";
            iv[jss::Flags] = hsfOVERRIDE;
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("Normal hook DELETE"),
                HSFEE);
            env.close();

            // check to ensure definition is deleted and hooks object too
            auto const def = env.le(accept_keylet);
            auto const hook = env.le(keylet::hook(Account("alice").id()));

            BEAST_EXPECT(!def);
            BEAST_EXPECT(!hook);

        }

        // create four hooks then delete the second last one
        {
            // create
            {
                Json::Value jv =
                    ripple::test::jtx::hook(alice, {{
                            hso(accept_wasm),
                            hso(makestate_wasm),
                            hso(rollback_wasm),
                            hso(accept2_wasm)
                            }}, 0);
                env(jv,
                    M("Create four"),
                    HSFEE, ter(tesSUCCESS));
                env.close();
            }

            // delete third and check
            {
                Json::Value iv;
                iv[jss::CreateCode] = "";
                iv[jss::Flags] = hsfOVERRIDE;
                for (uint8_t i = 0; i < 4; ++i)
                    jv[jss::Hooks][i][jss::Hook] = Json::Value{};
                jv[jss::Hooks][2U][jss::Hook] = iv;

                env(jv,
                    M("Normal hooki DELETE (third pos)"),
                    HSFEE);
                env.close();


                // check the hook definitions are consistent with reference count
                // dropping to zero on the third
                auto const accept_def = env.le(accept_keylet);
                auto const rollback_def = env.le(rollback_keylet);
                auto const makestate_def = env.le(makestate_keylet);
                auto const accept2_def = env.le(accept2_keylet);

                BEAST_REQUIRE(accept_def);
                BEAST_EXPECT(!rollback_def);
                BEAST_REQUIRE(makestate_def);
                BEAST_REQUIRE(accept2_def);

                // check the hooks array is correct
                auto const hook = env.le(keylet::hook(Account("alice").id()));
                BEAST_REQUIRE(hook);

                auto const& hooks = hook->getFieldArray(sfHooks);
                BEAST_REQUIRE(hooks.size() == 4);

                // make sure only the third is deleted
                BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookHash));
                BEAST_REQUIRE(hooks[1].isFieldPresent(sfHookHash));
                BEAST_EXPECT(!hooks[2].isFieldPresent(sfHookHash));
                BEAST_REQUIRE(hooks[3].isFieldPresent(sfHookHash));

                // check hashes on the three remaining
                BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);
                BEAST_EXPECT(hooks[1].getFieldH256(sfHookHash) == makestate_hash);
                BEAST_EXPECT(hooks[3].getFieldH256(sfHookHash) == accept2_hash);

            }

            // delete rest and check
            {
                Json::Value iv;
                iv[jss::CreateCode] = "";
                iv[jss::Flags] = hsfOVERRIDE;
                for (uint8_t i = 0; i < 4; ++i)
                {
                    if (i != 2U)
                       jv[jss::Hooks][i][jss::Hook] = iv;
                    else
                       jv[jss::Hooks][i][jss::Hook] = Json::Value{};
                }

                env(jv,
                    M("Normal hook DELETE (first, second, fourth pos)"),
                    HSFEE);
                env.close();

                // check the hook definitions are consistent with reference count
                // dropping to zero on the third
                auto const accept_def = env.le(accept_keylet);
                auto const rollback_def = env.le(rollback_keylet);
                auto const makestate_def = env.le(makestate_keylet);
                auto const accept2_def = env.le(accept2_keylet);

                BEAST_EXPECT(!accept_def);
                BEAST_EXPECT(!rollback_def);
                BEAST_EXPECT(!makestate_def);
                BEAST_EXPECT(!accept2_def);

                // check the hooks object is gone
                auto const hook = env.le(keylet::hook(Account("alice").id()));
                BEAST_EXPECT(!hook);

            }
        }
    }

    void testNSDelete()
    {
        testcase("Checks malformed nsdelete operation");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        auto const bob = Account{"bob"};
        env.fund(XRP(10000), bob);

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        for (auto const& [key, value]:
            JSSMap {
                {jss::HookGrants, Json::arrayValue},
                {jss::HookParameters, Json::arrayValue},
                {jss::HookOn, "0"},
                {jss::HookApiVersion, "0"},
            })
        {
            Json::Value iv;
            iv[key] = value;
            iv[jss::Flags] = hsfNSDELETE;
            iv[jss::HookNamespace] = to_string(uint256{beast::zero});
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook NSDELETE operation cannot include: grants, params, hookon, apiversion"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        auto const key = uint256::fromVoid((std::array<uint8_t,32>{
            0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
            0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
            0x00U, 0x00U, 0x00U, 0x00U,    'k',   'e',  'y', 0x00U
        }).data());

        auto const ns = uint256::fromVoid((std::array<uint8_t,32>{
            0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
            0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
            0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
            0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU
        }).data());

        auto const stateKeylet =
            keylet::hookState(
                Account("alice").id(),
                key,
                ns);

        // create a namespace
        std::string ns_str = "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE";
        {
            // create hook
            Json::Value jv =
                ripple::test::jtx::hook(alice, {{hso(makestate_wasm)}}, 0);

            jv[jss::Hooks][0U][jss::Hook][jss::HookNamespace] = ns_str;
            env(jv, M("Create makestate hook"), HSFEE, ter(tesSUCCESS));
            env.close();

            // run hook
            env(pay(bob, alice, XRP(1)),
                M("Run create state hook"),
                fee(XRP(1)));
            env.close();


            // check if the hookstate object was created
            auto const hookstate = env.le(stateKeylet);
            BEAST_EXPECT(!!hookstate);

            // check if the value was set correctly
            auto const& data = hookstate->getFieldVL(sfHookStateData);
            BEAST_REQUIRE(data.size() == 6);
            BEAST_EXPECT(
                    data[0] == 'v' && data[1] == 'a' && data[2] == 'l' &&
                    data[3] == 'u' && data[4] == 'e' && data[5] == '\0');
        }

        // delete the namespace
        {
            Json::Value iv;
            iv[jss::Flags] = hsfNSDELETE;
            iv[jss::HookNamespace] = ns_str;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Normal NSDELETE operation"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            // ensure the hook is still installed
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);

            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() > 0);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == makestate_hash);

            // ensure the directory is gone
            auto const dirKeylet = keylet::hookStateDir(Account("alice").id(), ns);
            BEAST_EXPECT(!env.le(dirKeylet));

            // ensure the state object is gone
            BEAST_EXPECT(!env.le(stateKeylet));

        }

    }

    void testCreate()
    {
        testcase("Checks malformed create operation");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        env.fund(XRP(10000), bob);

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);


        // test normal create and missing override flag
        {
            env(ripple::test::jtx::hook(bob, {{hso(accept_wasm)}}, 0),
                    M("First set = tesSUCCESS"),
                    HSFEE, ter(tesSUCCESS));

            env(ripple::test::jtx::hook(bob, {{hso(accept_wasm)}}, 0),
                    M("Second set = tecREQUIRES_FLAG"),
                    HSFEE, ter(tecREQUIRES_FLAG));
            env.close();
        }

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        // payload too large
        {
            env(ripple::test::jtx::hook(alice, {{hso(long_wasm)}}, 0),
                M("If CreateCode is present, then it must be less than 64kib"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // namespace missing
        {
            Json::Value iv;
            iv[jss::CreateCode] = strHex(accept_wasm);
            iv[jss::HookApiVersion] = 0U;
            iv[jss::HookOn] = uint64_hex(0);
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("HSO Create operation must contain namespace"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // api version missing
        {
            Json::Value iv;
            iv[jss::CreateCode] = strHex(accept_wasm);
            iv[jss::HookNamespace] = to_string(uint256{beast::zero});
            iv[jss::HookOn] = uint64_hex(0);
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("HSO Create operation must contain api version"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // api version wrong
        {
            Json::Value iv;
            iv[jss::CreateCode] = strHex(accept_wasm);
            iv[jss::HookNamespace] = to_string(uint256{beast::zero});
            iv[jss::HookApiVersion] = 1U;
            iv[jss::HookOn] = uint64_hex(0);
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("HSO Create operation must contain valid api version"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // hookon missing
        {
            Json::Value iv;
            iv[jss::CreateCode] = strHex(accept_wasm);
            iv[jss::HookNamespace] = to_string(uint256{beast::zero});
            iv[jss::HookApiVersion] = 0U;
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("HSO Create operation must contain hookon"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // hook hash present
        {
            Json::Value jv =
                ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0);
            Json::Value iv = jv[jss::Hooks][0U];
            iv[jss::Hook][jss::HookHash] = to_string(uint256{beast::zero});
            jv[jss::Hooks][0U] = iv;
            env(jv,
                M("Cannot have both CreateCode and HookHash"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }


        // correctly formed
        {
            Json::Value jv =
                ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0);
            env(jv,
                M("Normal accept"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            auto const def = env.le(accept_keylet);
            auto const hook = env.le(keylet::hook(Account("alice").id()));

            // check if the hook definition exists
            BEAST_EXPECT(!!def);

            // check if the user account has a hooks object
            BEAST_EXPECT(!!hook);

            // check if the hook is correctly set at position 1
            BEAST_EXPECT(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() > 0);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            // check if the wasm binary was correctly set
            BEAST_EXPECT(def->isFieldPresent(sfCreateCode));
            auto const& wasm = def->getFieldVL(sfCreateCode);
            auto const wasm_hash = sha512Half_s(ripple::Slice(wasm.data(), wasm.size()));
            BEAST_EXPECT(wasm_hash == accept_hash);
        }

        // add a second hook
        {
            Json::Value jv =
                ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0);
            Json::Value iv = jv[jss::Hooks][0U];
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = Json::Value{};
            jv[jss::Hooks][1U] = iv;
            env(jv,
                M("Normal accept, second position"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            auto const def = env.le(accept_keylet);
            auto const hook = env.le(keylet::hook(Account("alice").id()));

            // check if the hook definition exists
            BEAST_EXPECT(!!def);

            // check if the user account has a hooks object
            BEAST_EXPECT(!!hook);

            // check if the hook is correctly set at position 2
            BEAST_EXPECT(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() > 1);
            BEAST_EXPECT(hooks[1].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[1].getFieldH256(sfHookHash) == accept_hash);

            // check if the reference count was correctly incremented
            BEAST_EXPECT(def->isFieldPresent(sfReferenceCount));
            // two references from alice, one from bob (first test above)
            BEAST_EXPECT(def->getFieldU64(sfReferenceCount) == 3ULL);
        }

        auto const rollback_hash = ripple::sha512Half_s(
            ripple::Slice(rollback_wasm.data(), rollback_wasm.size())
        );

        // test override
        {
            Json::Value jv =
                ripple::test::jtx::hook(alice, {{hso(rollback_wasm)}}, 0);
            jv[jss::Hooks][0U][jss::Hook][jss::Flags] = hsfOVERRIDE;
            env(jv,
                M("Rollback override"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            auto const rollback_def = env.le(rollback_keylet);
            auto const accept_def = env.le(accept_keylet);
            auto const hook = env.le(keylet::hook(Account("alice").id()));

            // check if the hook definition exists
            BEAST_EXPECT(rollback_def);
            BEAST_EXPECT(accept_def);

            // check if the user account has a hooks object
            BEAST_EXPECT(hook);

            // check if the hook is correctly set at position 1
            BEAST_EXPECT(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() > 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == rollback_hash);
            BEAST_EXPECT(hooks[1].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[1].getFieldH256(sfHookHash) == accept_hash);

            // check if the wasm binary was correctly set
            BEAST_EXPECT(rollback_def->isFieldPresent(sfCreateCode));
            auto const& wasm = rollback_def->getFieldVL(sfCreateCode);
            auto const wasm_hash = sha512Half_s(ripple::Slice(wasm.data(), wasm.size()));
            BEAST_EXPECT(wasm_hash == rollback_hash);

            // check if the reference count was correctly incremented
            BEAST_EXPECT(rollback_def->isFieldPresent(sfReferenceCount));
            BEAST_EXPECT(rollback_def->getFieldU64(sfReferenceCount) == 1ULL);

            // check if the reference count was correctly decremented
            BEAST_EXPECT(accept_def->isFieldPresent(sfReferenceCount));
            BEAST_EXPECT(accept_def->getFieldU64(sfReferenceCount) == 2ULL);
        }
    }

    void testUpdate()
    {
        testcase("Checks malformed update operation");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        auto const bob = Account{"bob"};
        env.fund(XRP(10000), bob);

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        // first create the hook
        {
            Json::Value iv;
            iv[jss::CreateCode] = strHex(accept_wasm);
            iv[jss::HookNamespace] = to_string(uint256{beast::zero});
            iv[jss::HookApiVersion] = 0U;
            iv[jss::HookOn] = uint64_hex(0);
            iv[jss::HookParameters] = Json::Value{Json::arrayValue};
            iv[jss::HookParameters][0U] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter][jss::HookParameterName ] = "AAAAAAAAAAAA";
            iv[jss::HookParameters][0U][jss::HookParameter][jss::HookParameterValue] = "BBBBBB";

            iv[jss::HookParameters][1U] = Json::Value{};
            iv[jss::HookParameters][1U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][1U][jss::HookParameter][jss::HookParameterName ] = "CAFE";
            iv[jss::HookParameters][1U][jss::HookParameter][jss::HookParameterValue] = "FACADE";

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Create accept"),
                HSFEE, ter(tesSUCCESS));
            env.close();
        }

        // all alice operations below are then updates

        // must not specify override flag
        {
            Json::Value iv;
            iv[jss::Flags] = hsfOVERRIDE;
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("Override flag not allowed on update"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // must not specify NSDELETE unless also Namespace
        {
            Json::Value iv;
            iv[jss::Flags] = hsfNSDELETE;
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("NSDELETE flag not allowed on update unless HookNamespace also present"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }


        // api version not allowed in update
        {
            Json::Value iv;
            iv[jss::HookApiVersion] = 0U;
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("ApiVersion not allowed in update"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // try individually updating the various allowed fields
        {
            Json::Value params{Json::arrayValue};
            params[0U][jss::HookParameter] = Json::Value{};
            params[0U][jss::HookParameter][jss::HookParameterName] = "CAFE";
            params[0U][jss::HookParameter][jss::HookParameterValue] = "BABE";


            Json::Value grants{Json::arrayValue};
            grants[0U][jss::HookGrant] = Json::Value{};
            grants[0U][jss::HookGrant][jss::HookHash] = accept_hash_str;

            for (auto const& [key, value]:
                JSSMap{
                    {jss::HookOn, "1"},
                    {jss::HookNamespace, "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE"},
                    {jss::HookParameters, params},
                    {jss::HookGrants, grants}
                })
            {
                Json::Value iv;
                iv[key] = value;
                jv[jss::Hooks][0U] = Json::Value{};
                jv[jss::Hooks][0U][jss::Hook] = iv;

                env(jv,
                    M("Normal update"),
                    HSFEE, ter(tesSUCCESS));
                env.close();

            }

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);
            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);


            // check all fields were updated to correct values
            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookOn));
            BEAST_EXPECT(hooks[0].getFieldU64(sfHookOn) == 1ULL);

            auto const ns = uint256::fromVoid((std::array<uint8_t,32>{
                0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU
            }).data());
            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookNamespace));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookNamespace) == ns);

            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookParameters));
            const auto& p = hooks[0].getFieldArray(sfHookParameters);
            BEAST_REQUIRE(p.size() == 1);
            BEAST_REQUIRE(p[0].isFieldPresent(sfHookParameterName));
            BEAST_REQUIRE(p[0].isFieldPresent(sfHookParameterValue));

            const auto pn = p[0].getFieldVL(sfHookParameterName);
            BEAST_REQUIRE(pn.size() == 2);
            BEAST_REQUIRE(pn[0] == 0xCAU && pn[1] == 0xFEU);

            const auto pv = p[0].getFieldVL(sfHookParameterValue);
            BEAST_REQUIRE(pv.size() == 2);
            BEAST_REQUIRE(pv[0] == 0xBAU&& pv[1] == 0xBEU);

            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookGrants));
            const auto& g = hooks[0].getFieldArray(sfHookGrants);
            BEAST_REQUIRE(g.size() == 1);
            BEAST_REQUIRE(g[0].isFieldPresent(sfHookHash));
            BEAST_REQUIRE(g[0].getFieldH256(sfHookHash) == accept_hash);
        }

        // reset hookon and namespace to defaults
        {
            for (auto const& [key, value]:
                JSSMap{
                    {jss::HookOn, "0"},
                    {jss::HookNamespace, to_string(uint256{beast::zero})}
                })
            {
                Json::Value iv;
                iv[key] = value;
                jv[jss::Hooks][0U] = Json::Value{};
                jv[jss::Hooks][0U][jss::Hook] = iv;

                env(jv,
                    M("Reset to default"),
                    HSFEE, ter(tesSUCCESS));
                env.close();
            }

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);
            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            // ensure the two fields are now absent (because they were reset to the defaults on the hook def)
            BEAST_EXPECT(!hooks[0].isFieldPresent(sfHookOn));
            BEAST_EXPECT(!hooks[0].isFieldPresent(sfHookNamespace));
        }

        // add three additional parameters
        std::map<ripple::Blob, ripple::Blob> params {
            {{0xFEU, 0xEDU, 0xFAU, 0xCEU}, {0xF0U, 0x0DU}},
            {{0xA0U}, {0xB0U}},
            {{0xCAU, 0xFEU}, {0xBAU, 0xBEU}},
            {{0xAAU}, {0xBBU, 0xCCU}}
        };
        {

            Json::Value iv;
            iv[jss::HookParameters] = Json::Value{Json::arrayValue};
            iv[jss::HookParameters][0U] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter][jss::HookParameterName ] = "FEEDFACE";
            iv[jss::HookParameters][0U][jss::HookParameter][jss::HookParameterValue] = "F00D";

            iv[jss::HookParameters][1U] = Json::Value{};
            iv[jss::HookParameters][1U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][1U][jss::HookParameter][jss::HookParameterName ] = "A0";
            iv[jss::HookParameters][1U][jss::HookParameter][jss::HookParameterValue] = "B0";

            iv[jss::HookParameters][2U] = Json::Value{};
            iv[jss::HookParameters][2U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][2U][jss::HookParameter][jss::HookParameterName ] = "AA";
            iv[jss::HookParameters][2U][jss::HookParameter][jss::HookParameterValue] = "BBCC";

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Add three parameters"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);
            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            // check all the previous parameters plus the new ones
            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookParameters));
            const auto& p = hooks[0].getFieldArray(sfHookParameters);
            BEAST_REQUIRE(p.size() == params.size());

            std::set<ripple::Blob> already;

            for (uint8_t i = 0; i < params.size(); ++i)
            {
                const auto pn = p[i].getFieldVL(sfHookParameterName);
                const auto pv = p[i].getFieldVL(sfHookParameterValue);

                // make sure it's not a duplicate entry
                BEAST_EXPECT(already.find(pn) == already.end());

                // make  sure it exists
                BEAST_EXPECT(params.find(pn) != params.end());

                // make sure the value matches
                BEAST_EXPECT(params[pn] == pv);
                already.emplace(pn);
            }

        }

        // try to reset CAFE parameter to default
        {
            Json::Value iv;
            iv[jss::HookParameters] = Json::Value{Json::arrayValue};
            iv[jss::HookParameters][0U] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter][jss::HookParameterName ] = "CAFE";

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Reset cafe param to default using Absent Value"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);
            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            params.erase({0xCAU, 0xFEU});

            // check there right number of parameters exist
            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookParameters));
            const auto& p = hooks[0].getFieldArray(sfHookParameters);
            BEAST_REQUIRE(p.size() == params.size());

            // and that they still have the expected values and that there are no duplicates
            std::set<ripple::Blob> already;
            for (uint8_t i = 0; i < params.size(); ++i)
            {
                const auto pn = p[i].getFieldVL(sfHookParameterName);
                const auto pv = p[i].getFieldVL(sfHookParameterValue);

                // make sure it's not a duplicate entry
                BEAST_EXPECT(already.find(pn) == already.end());

                // make  sure it exists
                BEAST_EXPECT(params.find(pn) != params.end());

                // make sure the value matches
                BEAST_EXPECT(params[pn] == pv);
                already.emplace(pn);
            }
        }

        // now re-add CAFE parameter but this time as an explicit blank (Empty value)
        {
            Json::Value iv;
            iv[jss::HookParameters] = Json::Value{Json::arrayValue};
            iv[jss::HookParameters][0U] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter][jss::HookParameterName ] = "CAFE";
            iv[jss::HookParameters][0U][jss::HookParameter][jss::HookParameterValue ] = "";

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Set cafe param to blank using Empty Value"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);
            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            params[Blob{0xCAU, 0xFEU}]= Blob{};

            // check there right number of parameters exist
            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookParameters));
            const auto& p = hooks[0].getFieldArray(sfHookParameters);
            BEAST_REQUIRE(p.size() == params.size());

            // and that they still have the expected values and that there are no duplicates
            std::set<ripple::Blob> already;
            for (uint8_t i = 0; i < params.size(); ++i)
            {
                const auto pn = p[i].getFieldVL(sfHookParameterName);
                const auto pv = p[i].getFieldVL(sfHookParameterValue);

                // make sure it's not a duplicate entry
                BEAST_EXPECT(already.find(pn) == already.end());

                // make  sure it exists
                BEAST_EXPECT(params.find(pn) != params.end());

                // make sure the value matches
                BEAST_EXPECT(params[pn] == pv);
                already.emplace(pn);
            }
        }


        // try to delete all parameters (reset to defaults) using EMA (Empty Parameters Array)
        {
            Json::Value iv;
            iv[jss::HookParameters] = Json::Value{Json::arrayValue};

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Unset all params on hook"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);
            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            // check there right number of parameters exist
            BEAST_REQUIRE(!hooks[0].isFieldPresent(sfHookParameters));
        }




        // try to set each type of field on a non existent hook
        {
            Json::Value params{Json::arrayValue};
            params[0U][jss::HookParameter] = Json::Value{};
            params[0U][jss::HookParameter][jss::HookParameterName] = "CAFE";
            params[0U][jss::HookParameter][jss::HookParameterValue] = "BABE";


            Json::Value grants{Json::arrayValue};
            grants[0U][jss::HookGrant] = Json::Value{};
            grants[0U][jss::HookGrant][jss::HookHash] = accept_hash_str;

            for (auto const& [key, value]:
                JSSMap{
                    {jss::HookOn, "1"},
                    {jss::HookNamespace, "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE"},
                    {jss::HookParameters, params},
                    {jss::HookGrants, grants}
                })
            {
                Json::Value iv;
                iv[key] = value;
                jv[jss::Hooks][0U] = Json::Value{};
                jv[jss::Hooks][0U][jss::Hook] = Json::Value{};
                jv[jss::Hooks][1U] = Json::Value{};
                jv[jss::Hooks][1U][jss::Hook] = iv;

                env(jv,
                    M("Invalid update on non existent hook"),
                    HSFEE, ter(tecNO_ENTRY));
                env.close();
            }

            // ensure hook still exists and that there was no created new entry
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);
            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);
        }

        // test adding multiple grants
        {
            {
                // add a second hook
                env(ripple::test::jtx::hook(alice, {{{}, hso(accept_wasm)}}, 0),
                    M("Add second hook"),
                    HSFEE, ter(tesSUCCESS));
            }

            Json::Value grants{Json::arrayValue};
            grants[0U][jss::HookGrant] = Json::Value{};
            grants[0U][jss::HookGrant][jss::HookHash] = rollback_hash_str;
            grants[0U][jss::HookGrant][jss::Authorize] = bob.human();

            grants[1U][jss::HookGrant] = Json::Value{};
            grants[1U][jss::HookGrant][jss::HookHash] = accept_hash_str;

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = Json::objectValue;
            jv[jss::Hooks][1U] = Json::Value{};
            jv[jss::Hooks][1U][jss::Hook] = Json::Value{};
            jv[jss::Hooks][1U][jss::Hook][jss::HookGrants] = grants;

            env(jv,
                M("Add grants"),
                HSFEE);
            env.close();

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);

            std::cout << *hook << "\n";

            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 2);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            // check there right number of grants exist
            // hook 0 should have 1 grant
            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookGrants));
            BEAST_REQUIRE(hooks[0].getFieldArray(sfHookGrants).size() == 1);
            // hook 1 should have 2 grants
            {
                BEAST_REQUIRE(hooks[1].isFieldPresent(sfHookGrants));
                auto const& grants = hooks[1].getFieldArray(sfHookGrants);
                BEAST_REQUIRE(grants.size() == 2);

                BEAST_REQUIRE(grants[0].isFieldPresent(sfHookHash));
                BEAST_REQUIRE(grants[0].isFieldPresent(sfAuthorize));
                BEAST_REQUIRE(grants[1].isFieldPresent(sfHookHash));
                BEAST_EXPECT(!grants[1].isFieldPresent(sfAuthorize));


                BEAST_EXPECT(grants[0].getFieldH256(sfHookHash) == rollback_hash);
                BEAST_EXPECT(grants[0].getAccountID(sfAuthorize) == bob.id());

                BEAST_EXPECT(grants[1].getFieldH256(sfHookHash) == accept_hash);
            }

        }

        // update grants
        {
            Json::Value grants{Json::arrayValue};
            grants[0U][jss::HookGrant] = Json::Value{};
            grants[0U][jss::HookGrant][jss::HookHash] = makestate_hash_str;

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = Json::objectValue;
            jv[jss::Hooks][1U] = Json::Value{};
            jv[jss::Hooks][1U][jss::Hook] = Json::Value{};
            jv[jss::Hooks][1U][jss::Hook][jss::HookGrants] = grants;

            env(jv,
                M("update grants"),
                HSFEE);
            env.close();

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);

            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 2);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            // check there right number of grants exist
            // hook 1 should have 1 grant
            {
                BEAST_REQUIRE(hooks[1].isFieldPresent(sfHookGrants));
                auto const& grants = hooks[1].getFieldArray(sfHookGrants);
                BEAST_REQUIRE(grants.size() == 1);
                BEAST_REQUIRE(grants[0].isFieldPresent(sfHookHash));
                BEAST_EXPECT(grants[0].getFieldH256(sfHookHash) == makestate_hash);
            }

        }

        // use an empty grants array to reset the grants
        {
            jv[jss::Hooks][0U] = Json::objectValue;
            jv[jss::Hooks][0U][jss::Hook] = Json::objectValue;
            jv[jss::Hooks][1U] = Json::Value{};
            jv[jss::Hooks][1U][jss::Hook] = Json::Value{};
            jv[jss::Hooks][1U][jss::Hook][jss::HookGrants] = Json::arrayValue;

            env(jv,
                M("clear grants"),
                HSFEE);
            env.close();

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);

            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 2);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            // check there right number of grants exist
            // hook 1 should have 0 grants
            BEAST_REQUIRE(!hooks[1].isFieldPresent(sfHookGrants));

        }
    }


    void testInferHookSetOperation()
    {
        testcase("Test operation inference");

        // hsoNOOP
        {
            STObject hso{sfHook};
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoNOOP);
        }

        // hsoCREATE
        {
            STObject hso{sfHook};
            hso.setFieldVL(sfCreateCode, {1});  // non-empty create code
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoCREATE);
        }

        // hsoDELETE
        {
            STObject hso{sfHook};
            hso.setFieldVL(sfCreateCode, ripple::Blob{});  // empty create code
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoDELETE);
        }

        // hsoINSTALL
        {
            STObject hso{sfHook};
            hso.setFieldH256(sfHookHash, uint256{beast::zero});  // all zeros hook hash
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoINSTALL);
        }

        // hsoNSDELETE
        {
            STObject hso{sfHook};
            hso.setFieldH256(sfHookNamespace, uint256{beast::zero});  // all zeros hook hash
            hso.setFieldU32(sfFlags, hsfNSDELETE);
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoNSDELETE);

        }

        // hsoUPDATE
        {
            STObject hso{sfHook};
            hso.setFieldU64(sfHookOn, 1LLU);
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoUPDATE);
        }

        //hsoINVALID
        {
            STObject hso{sfHook};
            hso.setFieldVL(sfCreateCode, {1});  // non-empty create code
            hso.setFieldH256(sfHookHash, uint256{beast::zero});  // all zeros hook hash
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoINVALID);
        }
    }

    void
    testWasm()
    {
        testcase("Checks malformed hook binaries");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        env(ripple::test::jtx::hook(alice, {{hso(noguard_wasm)}}, 0),
                M("Must import guard"),
                HSFEE, ter(temMALFORMED));

        env(ripple::test::jtx::hook(alice, {{hso(illegalfunc_wasm)}}, 0),
                M("Must only contain hook and cbak"),
                HSFEE, ter(temMALFORMED));
    }

    void
    test_accept()
    {
        testcase("Test accept() hookapi");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        env(ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0),
            M("Install Accept Hook"),
            HSFEE);
        env.close();

        env(pay(bob, alice, XRP(1)),
            M("Test Accept Hook"),
            fee(XRP(1)));
        env.close();

    }

    void
    test_rollback()
    {
        testcase("Test rollback() hookapi");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        env(ripple::test::jtx::hook(alice, {{hso(rollback_wasm)}}, 0),
            M("Install Rollback Hook"),
            HSFEE);
        env.close();

        env(pay(bob, alice, XRP(1)),
            M("Test Rollback Hook"),
            fee(XRP(1)), ter(tecHOOK_REJECTED));
        env.close();
    }

    void
    testGuards()
    {
        testcase("Test guards");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        // test a simple loop without a guard call
        {
            TestHook
            hook =
            wasm[R"[test.hook](
                (module
                  (type (;0;) (func (param i32 i32) (result i64)))
                  (type (;1;) (func (param i32 i32) (result i32)))
                  (type (;2;) (func (param i32) (result i64)))
                  (import "env" "hook_account" (func (;0;) (type 0)))
                  (import "env" "_g" (func (;1;) (type 1)))
                  (func (;2;) (type 2) (param i32) (result i64)
                    (local i32)
                    global.get 0
                    i32.const 32
                    i32.sub
                    local.tee 1
                    global.set 0
                    loop (result i64)  ;; label = @1
                      local.get 1
                      i32.const 20
                      call 0
                      drop
                      br 0 (;@1;)
                    end)
                  (memory (;0;) 2)
                  (global (;0;) (mut i32) (i32.const 66560))
                  (global (;1;) i32 (i32.const 1024))
                  (global (;2;) i32 (i32.const 1024))
                  (global (;3;) i32 (i32.const 66560))
                  (global (;4;) i32 (i32.const 1024))
                  (export "memory" (memory 0))
                  (export "hook" (func 2)))
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook)}}, 0),
                M("Loop 1 no guards"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // same loop again but with a guard call
        {
            TestHook
            hook =
            wasm[R"[test.hook](
                (module
                  (type (;0;) (func (param i32 i32) (result i64)))
                  (type (;1;) (func (param i32 i32) (result i32)))
                  (type (;2;) (func (param i32) (result i64)))
                  (import "env" "hook_account" (func (;0;) (type 0)))
                  (import "env" "_g" (func (;1;) (type 1)))
                  (func (;2;) (type 2) (param i32) (result i64)
                    (local i32)
                    global.get 0
                    i32.const 32
                    i32.sub
                    local.tee 1
                    global.set 0
                    loop (result i64)  ;; label = @1
                      i32.const 1
                      i32.const 1
                      call 1
                      drop
                      local.get 1
                      i32.const 20
                      call 0
                      drop
                      br 0 (;@1;)
                    end)
                  (memory (;0;) 2)
                  (global (;0;) (mut i32) (i32.const 66560))
                  (global (;1;) i32 (i32.const 1024))
                  (global (;2;) i32 (i32.const 1024))
                  (global (;3;) i32 (i32.const 66560))
                  (global (;4;) i32 (i32.const 1024))
                  (export "memory" (memory 0))
                  (export "hook" (func 2)))
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook)}}, 0),
                M("Loop 1 with guards"),
                HSFEE);
            env.close();
        }

        // simple looping, c
        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t hook_account (uint32_t, uint32_t);
                int64_t hook(uint32_t reserved )
                {
                    uint8_t acc[20];
                    for (int i = 0; GUARD(10), i < 10; ++i)
                        hook_account(acc, 20);

                    return accept(0,0,2);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("Loop 2 in C"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("Test Loop 2"),
                fee(XRP(1)));
            env.close();
        }

        // complex looping, c
        {
            TestHook
            hook =
            wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_account (uint32_t, uint32_t);
            int64_t hook(uint32_t reserved)
            {
                uint8_t acc[20];
                // guards should be computed by:
                // (this loop iterations + 1) * (each parent loop's iteration's + 0)
                for (int i = 0; i < 10; ++i)
                {
                    _g(1, 11);
                    for (int j = 0; j < 2; ++j)
                    {
                        _g(2, 30);
                        for (int k = 0;  k < 5; ++k)
                        {
                            _g(3, 120);
                            hook_account(acc, 20);
                        }
                        for (int k = 0;  k < 5; ++k)
                        {
                            _g(4, 120);
                            hook_account(acc, 20);
                        }
                    }
                }
                return accept(0,0,2);
            }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("Loop 3 in C"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("Test Loop 3"),
                fee(XRP(1)));
            env.close();
        }

        // complex looping missing a guard
        {
            TestHook
            hook =
            wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_account (uint32_t, uint32_t);
            int64_t hook(uint32_t reserved)
            {
                uint8_t acc[20];
                // guards should be computed by:
                // (this loop iterations + 1) * (each parent loop's iteration's + 0)
                for (int i = 0; i < acc[0]; ++i)
                {
                    _g(1, 11);
                    for (int j = 0; j < acc[1]; ++j)
                    {
                        // guard missing here
                        hook_account(acc, 20);
                        for (int k = 0;  k < acc[2]; ++k)
                        {
                            _g(3, 120);
                            hook_account(acc, 20);
                        }
                        for (int k = 0;  k < acc[3]; ++k)
                        {
                            _g(4, 120);
                            hook_account(acc, 20);
                        }
                    }
                }
                return accept(0,0,2);
            }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("Loop 4 in C"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }
    }

    void
    test_emit()
    {
    }

    void
    test_etxn_burden()
    {
    }

    void
    test_etxn_details()
    {
    }

    void
    test_etxn_fee_base()
    {
    }

    void
    test_etxn_generation()
    {
    }

    void
    test_etxn_nonce()
    {
    }

    void
    test_etxn_reserve()
    {
    }

    void
    test_fee_base()
    {
    }

    void
    test_float_compare()
    {
        testcase("Test float_compare");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_one (void);
                extern int64_t float_compare(int64_t, int64_t, uint32_t);
                #define EQ   0b001U
                #define LT   0b010U
                #define GT   0b100U
                #define LTE  0b011U
                #define GTE  0b101U
                #define NEQ  0b110U
                #define ASSERT(x)\
                    if ((x) != 1)\
                        rollback(0,0,__LINE__)
                #define INVALID_ARGUMENT -7
                #define INVALID_FLOAT -10024
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    int64_t result = 0;

                    // test invalid floats
                    {
                        ASSERT(float_compare(-1,-2, EQ) == INVALID_FLOAT);
                        ASSERT(float_compare( 0,-2, EQ) == INVALID_FLOAT);
                        ASSERT(float_compare(-1, 0, EQ) == INVALID_FLOAT);
                    }

                    // test invalid flags
                    {
                        // flag 8 doesnt exist
                        ASSERT(float_compare(0,0,   0b1000U) == INVALID_ARGUMENT);
                        // flag 16 doesnt exist
                        ASSERT(float_compare(0,0,  0b10000U) == INVALID_ARGUMENT);
                        // every flag except the valid ones
                        ASSERT(float_compare(0,0,  ~0b111UL) == INVALID_ARGUMENT);
                        // all valid flags combined is invalid too
                        ASSERT(float_compare(0,0,   0b111UL) == INVALID_ARGUMENT);
                        // no flags is also invalid
                        ASSERT(float_compare(0,0,   0) == INVALID_ARGUMENT);
                    }

                    // test logic
                    {
                        ASSERT(float_compare(0,0,EQ));
                        ASSERT(float_compare(0, float_one(), LT));
                        ASSERT(float_compare(0, float_one(), GT) == 0);
                        ASSERT(float_compare(0, float_one(), GTE) == 0);
                        ASSERT(float_compare(0, float_one(), LTE));
                        ASSERT(float_compare(0, float_one(), NEQ));

                        int64_t large_negative = 1622844335003378560LL; /* -154846915           */
                        int64_t small_negative = 1352229899321148800LL; /* -1.15001111e-7       */
                        int64_t small_positive = 5713898440837102138LL; /* 3.33411333131321e-21 */
                        int64_t large_positive = 7749425685711506120LL; /* 3.234326634253e+92   */

                        // large negative < small negative
                        ASSERT(float_compare(large_negative, small_negative, LT));
                        ASSERT(float_compare(large_negative, small_negative, LTE));
                        ASSERT(float_compare(large_negative, small_negative, NEQ));
                        ASSERT(float_compare(large_negative, small_negative, GT) == 0);
                        ASSERT(float_compare(large_negative, small_negative, GTE) == 0);
                        ASSERT(float_compare(large_negative, small_negative, EQ) == 0);

                        // large_negative < large positive
                        ASSERT(float_compare(large_negative, large_positive, LT));
                        ASSERT(float_compare(large_negative, large_positive, LTE));
                        ASSERT(float_compare(large_negative, large_positive, NEQ));
                        ASSERT(float_compare(large_negative, large_positive, GT) == 0);
                        ASSERT(float_compare(large_negative, large_positive, GTE) == 0);
                        ASSERT(float_compare(large_negative, large_positive, EQ) == 0);

                        // small_negative < small_positive
                        ASSERT(float_compare(small_negative, small_positive, LT));
                        ASSERT(float_compare(small_negative, small_positive, LTE));
                        ASSERT(float_compare(small_negative, small_positive, NEQ));
                        ASSERT(float_compare(small_negative, small_positive, GT) == 0);
                        ASSERT(float_compare(small_negative, small_positive, GTE) == 0);
                        ASSERT(float_compare(small_negative, small_positive, EQ) == 0);

                        // small positive < large positive
                        ASSERT(float_compare(small_positive, large_positive, LT));
                        ASSERT(float_compare(small_positive, large_positive, LTE));
                        ASSERT(float_compare(small_positive, large_positive, NEQ));
                        ASSERT(float_compare(small_positive, large_positive, GT) == 0);
                        ASSERT(float_compare(small_positive, large_positive, GTE) == 0);
                        ASSERT(float_compare(small_positive, large_positive, EQ) == 0);

                        // small negative < 0
                        ASSERT(float_compare(small_negative, 0, LT));

                        // large negative < 0
                        ASSERT(float_compare(large_negative, 0, LT));

                        // small positive > 0
                        ASSERT(float_compare(small_positive, 0, GT));

                        // large positive > 0
                        ASSERT(float_compare(large_positive, 0, GT));
                    }

                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_compare"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_compare"),
                fee(XRP(1)));
            env.close();
        }

    }

    void
    test_float_divide()
    {
        testcase("Test float_divide");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_divide (int64_t, int64_t);
                extern int64_t float_one (void);
                #define INVALID_FLOAT -10024
                #define DIVISION_BY_ZERO -25
                #define XFL_OVERFLOW -30
                #define ASSERT(x)\
                    if (!(x))\
                        rollback(0,0,__LINE__);
                extern int64_t float_compare(int64_t, int64_t, uint32_t);
                extern int64_t float_negate(int64_t);
                extern int64_t float_sum(int64_t, int64_t);
                extern int64_t float_mantissa(int64_t);
                extern int64_t float_exponent(int64_t);
                #define ASSERT_EQUAL(x, y)\
                {\
                    int64_t px = (x);\
                    int64_t py = (y);\
                    int64_t mx = float_mantissa(px);\
                    int64_t my = float_mantissa(py);\
                    int32_t diffexp = float_exponent(px) - float_exponent(py);\
                    if (diffexp == 1)\
                        mx *= 10LL;\
                    if (diffexp == -1)\
                        my *= 10LL;\
                    int64_t diffman = mx - my;\
                    if (diffman < 0) diffman *= -1LL;\
                    if (diffexp < 0) diffexp *= -1;\
                    if (diffexp > 1 || diffman > 5000000)\
                        rollback((uint32_t) #x, sizeof(#x), __LINE__);\
                }
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // ensure invalid xfl are not accepted
                    ASSERT(float_divide(-1, float_one()) == INVALID_FLOAT);

                    // divide by 0
                    ASSERT(float_divide(float_one(), 0) == DIVISION_BY_ZERO);
                    ASSERT(float_divide(0, float_one()) == 0);

                    // check 1
                    ASSERT(float_divide(float_one(), float_one()) == float_one());
                    ASSERT(float_divide(float_one(), float_negate(float_one())) == float_negate(float_one()));
                    ASSERT(float_divide(float_negate(float_one()), float_one()) == float_negate(float_one()));
                    ASSERT(float_divide(float_negate(float_one()), float_negate(float_one())) == float_one());

                    // 1 / 10 = 0.1
                    ASSERT_EQUAL(float_divide(float_one(), 6107881094714392576LL), 6071852297695428608LL);

                    // 123456789 / 1623 = 76067.0295749
                    ASSERT_EQUAL(float_divide(6234216452170766464LL, 6144532891733356544LL), 6168530993200328528LL);

                    // -1.245678451111 / 1.3546984132111e+42 = -9.195245517106014e-43
                    ASSERT_EQUAL(float_divide(1478426356228633688LL, 6846826132016365020LL), 711756787386903390LL);

                    // 9.134546514878452e-81 / 1
                    ASSERT(float_divide(4638834963451748340LL, float_one()) == 4638834963451748340LL);

                    // 9.134546514878452e-81 / 1.41649684651e+75 = (underflow 0)
                    ASSERT(float_divide(4638834963451748340LL, 7441363081262569392LL) == 0);

                    // 1.3546984132111e+42 / 9.134546514878452e-81  = XFL_OVERFLOW
                    ASSERT(float_divide(6846826132016365020LL, 4638834963451748340LL) == XFL_OVERFLOW);

                    ASSERT_EQUAL(
                        float_divide(
                            3121244226425810900LL /* -4.753284285427668e+91 */,
                            2135203055881892282LL /* -9.50403176301817e+36 */),
                        7066645550312560102LL /* 5.001334595622374e+54 */);
                    ASSERT_EQUAL(
                        float_divide(
                            2473507938381460320LL /* -5.535342582428512e+55 */,
                            6365869885731270068LL /* 6787211884129716 */),
                        2187897766692155363LL /* -8.155547044835299e+39 */);
                    ASSERT_EQUAL(
                        float_divide(
                            1716271542690607496LL /* -49036842898190.16 */,
                            3137794549622534856LL /* -3.28920897266964e+92 */),
                        4667220053951274769LL /* 1.490839995440913e-79 */);
                    ASSERT_EQUAL(
                        float_divide(
                            1588045991926420391LL /* -2778923.092005799 */,
                            5933338827267685794LL /* 6.601717648113058e-9 */),
                        1733591650950017206LL /* -420939403974674.2 */);
                    ASSERT_EQUAL(
                        float_divide(
                            5880783758174228306LL /* 8.089844083101523e-12 */,
                            1396720886139976383LL /* -0.00009612200909863615 */),
                        1341481714205255877LL /* -8.416224503589061e-8 */);
                    ASSERT_EQUAL(
                        float_divide(
                            5567703563029955929LL /* 1.254423600022873e-29 */,
                            2184969513100691140LL /* -5.227293453371076e+39 */),
                        236586937995245543LL /* -2.399757371979751e-69 */);
                    ASSERT_EQUAL(
                        float_divide(
                            7333313065548121054LL /* 1.452872188953566e+69 */,
                            1755926008837497886LL /* -8529353417745438 */),
                        2433647177826281173LL /* -1.703379046213333e+53 */);
                    ASSERT_EQUAL(
                        float_divide(
                            1172441975040622050LL /* -1.50607192429309e-17 */,
                            6692015311011173216LL /* 8.673463993357152e+33 */),
                        560182767210134346LL /* -1.736413416192842e-51 */);
                    ASSERT_EQUAL(
                        float_divide(
                            577964843368607493LL /* -1.504091065184005e-50 */,
                            6422931182144699580LL /* 9805312769113276000 */),
                        235721135837751035LL /* -1.533955214485243e-69 */);
                    ASSERT_EQUAL(
                        float_divide(
                            6039815413139899240LL /* 0.0049919124634346 */,
                            2117655488444284242LL /* -9.970862834892113e+35 */),
                        779625635892827768LL /* -5.006499985102456e-39 */);
                    ASSERT_EQUAL(
                        float_divide(
                            1353563835098586141LL /* -2.483946887437341e-7 */,
                            6450909070545770298LL /* 175440415122002600000 */),
                        992207753070525611LL /* -1.415835049016491e-27 */);
                    ASSERT_EQUAL(
                        float_divide(
                            6382158843584616121LL /* 50617712279937850 */,
                            5373794957212741595LL /* 5.504201387110363e-40 */),
                        7088854809772330055LL /* 9.196195545910343e+55 */);
                    ASSERT_EQUAL(
                        float_divide(
                            2056891719200540975LL /* -3.250289119594799e+32 */,
                            1754532627802542730LL /* -7135972382790282 */),
                        6381651867337939070LL /* 45547949813167340 */);
                    ASSERT_EQUAL(
                        float_divide(
                            5730152450208688630LL /* 1.573724193417718e-20 */,
                            1663581695074866883LL /* -62570322025.24355 */),
                        921249452789827075LL /* -2.515128806245891e-31 */);
                    ASSERT_EQUAL(
                        float_divide(
                            6234301156018475310LL /* 131927173.7708846 */,
                            2868710604383082256LL /* -4.4212413754468e+77 */),
                        219156721749007916LL /* -2.983939635224108e-70 */);
                    ASSERT_EQUAL(
                        float_divide(
                            2691125731495874243LL /* -6.980353583058627e+67 */,
                            7394070851520237320LL /* 8.16746263262388e+72 */),
                        1377640825464715759LL /* -0.000008546538744084975 */);
                    ASSERT_EQUAL(
                        float_divide(
                            5141867696142208039LL /* 7.764120939842599e-53 */,
                            5369434678231981897LL /* 1.143922406350665e-40 */),
                        5861466794943198400LL /* 6.7872793615536e-13 */);
                    ASSERT_EQUAL(
                        float_divide(
                            638296190872832492LL /* -7.792243040963052e-47 */,
                            5161669734904371378LL /* 9.551761192523954e-52 */),
                        1557396184145861422LL /* -81579.12330410798 */);
                    ASSERT_EQUAL(
                        float_divide(
                            2000727145906286285LL /* -1.128911353786061e+29 */,
                            2096625200460673392LL /* -6.954973360763248e+34 */),
                        5982403476503576795LL /* 0.000001623171355558107 */);
                    ASSERT_EQUAL(
                        float_divide(
                            640472838055334326LL /* -9.968890223464885e-47 */,
                            5189754252349396763LL /* 1.607481618585371e-50 */),
                        1537425431139169736LL /* -6201.557833201096 */);
                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_divide"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_divide"),
                fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_exponent()
    {
    }

    void
    test_float_exponent_set()
    {
    }

    void
    test_float_int()
    {
        testcase("Test float_int");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_int (int64_t, uint32_t, uint32_t);
                extern int64_t float_one (void);
                #define INVALID_FLOAT -10024
                #define INVALID_ARGUMENT -7
                #define CANT_RETURN_NEGATIVE -33
                #define TOO_BIG -3
                #define ASSERT(x)\
                    if (!(x))\
                        rollback(0,0,__LINE__);
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // ensure invalid xfl are not accepted
                    ASSERT(float_int(-1,0,0) == INVALID_FLOAT);

                    // check 1
                    ASSERT(float_int(float_one(), 0, 0) == 1LL);

                    // check 1.23e-20 always returns 0 (too small to display)
                    ASSERT(float_int(5729808726015270912LL,0,0) == 0);
                    ASSERT(float_int(5729808726015270912LL,15,0) == 0);
                    ASSERT(float_int(5729808726015270912LL,16,0) == INVALID_ARGUMENT);


                    ASSERT(float_int(float_one(), 15, 0) == 1000000000000000LL);
                    ASSERT(float_int(float_one(), 14, 0) == 100000000000000LL);
                    ASSERT(float_int(float_one(), 13, 0) == 10000000000000LL);
                    ASSERT(float_int(float_one(), 12, 0) == 1000000000000LL);
                    ASSERT(float_int(float_one(), 11, 0) == 100000000000LL);
                    ASSERT(float_int(float_one(), 10, 0) == 10000000000LL);
                    ASSERT(float_int(float_one(),  9, 0) == 1000000000LL);
                    ASSERT(float_int(float_one(),  8, 0) == 100000000LL);
                    ASSERT(float_int(float_one(),  7, 0) == 10000000LL);
                    ASSERT(float_int(float_one(),  6, 0) == 1000000LL);
                    ASSERT(float_int(float_one(),  5, 0) == 100000LL);
                    ASSERT(float_int(float_one(),  4, 0) == 10000LL);
                    ASSERT(float_int(float_one(),  3, 0) == 1000LL);
                    ASSERT(float_int(float_one(),  2, 0) == 100LL);
                    ASSERT(float_int(float_one(),  1, 0) == 10LL);
                    ASSERT(float_int(float_one(),  0, 0) == 1LL);

                    // normal upper limit on exponent
                    ASSERT(float_int(6360317241828374919LL, 0, 0) == 1234567981234567LL);

                    // ask for one decimal above limit
                    ASSERT(float_int(6360317241828374919LL, 1, 0) == TOO_BIG);

                    // ask for 15 decimals above limit
                    ASSERT(float_int(6360317241828374919LL, 15, 0) == TOO_BIG);

                    // every combination for 1.234567981234567
                    ASSERT(float_int(6090101264186145159LL,  0, 0) == 1LL);
                    ASSERT(float_int(6090101264186145159LL,  1, 0) == 12LL);
                    ASSERT(float_int(6090101264186145159LL,  2, 0) == 123LL);
                    ASSERT(float_int(6090101264186145159LL,  3, 0) == 1234LL);
                    ASSERT(float_int(6090101264186145159LL,  4, 0) == 12345LL);
                    ASSERT(float_int(6090101264186145159LL,  5, 0) == 123456LL);
                    ASSERT(float_int(6090101264186145159LL,  6, 0) == 1234567LL);
                    ASSERT(float_int(6090101264186145159LL,  7, 0) == 12345679LL);
                    ASSERT(float_int(6090101264186145159LL,  8, 0) == 123456798LL);
                    ASSERT(float_int(6090101264186145159LL,  9, 0) == 1234567981LL);
                    ASSERT(float_int(6090101264186145159LL, 10, 0) == 12345679812LL);
                    ASSERT(float_int(6090101264186145159LL, 11, 0) == 123456798123LL);
                    ASSERT(float_int(6090101264186145159LL, 12, 0) == 1234567981234LL);
                    ASSERT(float_int(6090101264186145159LL, 13, 0) == 12345679812345LL);
                    ASSERT(float_int(6090101264186145159LL, 14, 0) == 123456798123456LL);
                    ASSERT(float_int(6090101264186145159LL, 15, 0) == 1234567981234567LL);

                    // same with absolute parameter
                    ASSERT(float_int(1478415245758757255LL,  0, 1) == 1LL);
                    ASSERT(float_int(1478415245758757255LL,  1, 1) == 12LL);
                    ASSERT(float_int(1478415245758757255LL,  2, 1) == 123LL);
                    ASSERT(float_int(1478415245758757255LL,  3, 1) == 1234LL);
                    ASSERT(float_int(1478415245758757255LL,  4, 1) == 12345LL);
                    ASSERT(float_int(1478415245758757255LL,  5, 1) == 123456LL);
                    ASSERT(float_int(1478415245758757255LL,  6, 1) == 1234567LL);
                    ASSERT(float_int(1478415245758757255LL,  7, 1) == 12345679LL);
                    ASSERT(float_int(1478415245758757255LL,  8, 1) == 123456798LL);
                    ASSERT(float_int(1478415245758757255LL,  9, 1) == 1234567981LL);
                    ASSERT(float_int(1478415245758757255LL, 10, 1) == 12345679812LL);
                    ASSERT(float_int(1478415245758757255LL, 11, 1) == 123456798123LL);
                    ASSERT(float_int(1478415245758757255LL, 12, 1) == 1234567981234LL);
                    ASSERT(float_int(1478415245758757255LL, 13, 1) == 12345679812345LL);
                    ASSERT(float_int(1478415245758757255LL, 14, 1) == 123456798123456LL);
                    ASSERT(float_int(1478415245758757255LL, 15, 1) == 1234567981234567LL);

                    // neg xfl sans absolute parameter
                    ASSERT(float_int(1478415245758757255LL, 15, 0) == CANT_RETURN_NEGATIVE);

                    // 1.234567981234567e-16
                    ASSERT(float_int(5819885286543915399LL, 15, 0) == 1LL);
                    for (uint32_t i = 1; GUARD(15), i < 15; ++i)
                        ASSERT(float_int(5819885286543915399LL, i, 0) == 0);

                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_int"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_int"),
                fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_invert()
    {
        testcase("Test float_invert");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_invert (int64_t);
                extern int64_t float_one (void);
                #define INVALID_FLOAT -10024
                #define DIVISION_BY_ZERO -25
                #define TOO_BIG -3
                #define ASSERT(x)\
                    if (!(x))\
                        rollback(0,0,__LINE__);
                extern int64_t float_compare(int64_t, int64_t, uint32_t);
                extern int64_t float_negate(int64_t);
                extern int64_t float_sum(int64_t, int64_t);
                extern int64_t float_mantissa(int64_t);
                extern int64_t float_exponent(int64_t);
                #define ASSERT_EQUAL(x, y)\
                {\
                    int64_t px = (x);\
                    int64_t py = (y);\
                    int64_t mx = float_mantissa(px);\
                    int64_t my = float_mantissa(py);\
                    int32_t diffexp = float_exponent(px) - float_exponent(py);\
                    if (diffexp == 1)\
                        mx *= 10LL;\
                    if (diffexp == -1)\
                        my *= 10LL;\
                    int64_t diffman = mx - my;\
                    if (diffman < 0) diffman *= -1LL;\
                    if (diffexp < 0) diffexp *= -1;\
                    if (diffexp > 1 || diffman > 5000000)\
                        rollback((uint32_t) #x, sizeof(#x), __LINE__);\
                }
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // divide by 0
                    ASSERT(float_invert(0) == DIVISION_BY_ZERO);

                    // ensure invalid xfl are not accepted
                    ASSERT(float_invert(-1) == INVALID_FLOAT);

                    // check 1
                    ASSERT(float_invert(float_one()) == float_one());

                    // 1 / 10 = 0.1
                    ASSERT_EQUAL(float_invert(6107881094714392576LL), 6071852297695428608LL);

                    // 1 / 123 = 0.008130081300813009
                    ASSERT_EQUAL(float_invert(6126125493223874560LL), 6042953581977277649LL);

                    // 1 / 1234567899999999 = 8.100000008100007e-16
                    ASSERT_EQUAL(float_invert(6360317241747140351LL), 5808736320061298855LL);

                    // 1/ 1*10^-81 = 10**81
                    ASSERT_EQUAL(float_invert(4630700416936869888LL), 7540018576963469311LL);
                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_invert"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_invert"),
                fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_log()
    {
        testcase("Test float_log");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_log (int64_t float1);
                extern int64_t float_one (void);
                #define INVALID_ARGUMENT -7
                #define COMPLEX_NOT_SUPPORTED -39
                extern int64_t float_mantissa(int64_t);
                extern int64_t float_exponent(int64_t);
                #define ASSERT_EQUAL(x, y)\
                {\
                    int64_t px = (x);\
                    int64_t py = (y);\
                    int64_t mx = float_mantissa(px);\
                    int64_t my = float_mantissa(py);\
                    int32_t diffexp = float_exponent(px) - float_exponent(py);\
                    if (diffexp == 1)\
                        mx *= 10LL;\
                    if (diffexp == -1)\
                        my *= 10LL;\
                    int64_t diffman = mx - my;\
                    if (diffman < 0) diffman *= -1LL;\
                    if (diffexp < 0) diffexp *= -1;\
                    if (diffexp > 1 || diffman > 5000000)\
                        rollback((uint32_t) #x, sizeof(#x), __LINE__);\
                }
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // check 0 is not allowed
                    if (float_log(0) != INVALID_ARGUMENT)
                        rollback(0,0,__LINE__);

                    // log10( 846513684968451 ) = 14.92763398342338
                    ASSERT_EQUAL(float_log(6349533412187342878LL), 6108373858112734914LL);

                    // log10 ( -1000 ) = invalid (complex not supported)
                    if (float_log(1532223873305968640LL) != COMPLEX_NOT_SUPPORTED)
                        rollback(0,0,__LINE__);

                    // log10 (1000) == 3
                    ASSERT_EQUAL(float_log(6143909891733356544LL), 6091866696204910592LL);

                    // log10 (0.112381) == -0.949307107740766
                    ASSERT_EQUAL(float_log(6071976107695428608LL), 1468659350345448364LL);

                    // log10 (0.00000000000000001123) = -16.94962024373854221
                    ASSERT_EQUAL(float_log(5783744921543716864LL), 1496890038311378526LL);

                    // log10 (100000000000000000000000000000000000000000000000000000000000000) = 62
                    ASSERT_EQUAL(float_log(7206759403792793600LL), 6113081094714392576LL);
                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_log"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_log"),
                fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_mantissa()
    {
    }

    void
    test_float_mantissa_set()
    {
    }

    void
    test_float_mulratio()
    {
    }

    void
    test_float_multiply()
    {
    }

    void
    test_float_negate()
    {
        testcase("Test float_negate");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_one (void);
                extern int64_t float_negate(int64_t);
                #define ASSERT(x)\
                    if ((x) != 1)\
                        rollback(0,0,__LINE__)
                #define INVALID_FLOAT -10024
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    int64_t result = 0;

                    // test invalid floats
                    {
                        ASSERT(float_negate(-1) == INVALID_FLOAT);
                        ASSERT(float_negate(-11010191919LL) == INVALID_FLOAT);
                    }

                    // test canonical zero
                    ASSERT(float_negate(0) == 0);

                    // test double negation
                    {
                        ASSERT(float_negate(float_one()) != float_one());
                        ASSERT(float_negate(float_negate(float_one())) == float_one());
                    }

                    // test random numbers
                    {
                       // +/- 3.463476342523e+22
                       ASSERT(float_negate(6488646939756037240LL) == 1876960921328649336LL);

                       ASSERT(float_negate(float_one()) == 1478180677777522688LL);

                       ASSERT(float_negate(1838620299498162368LL) == 6450306317925550272LL);
                    }

                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_negate"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_negate"),
                fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_one()
    {
        testcase("Test float_one");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_one (void);
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);
                    int64_t f = float_one();
                    return
                        f == 6089866696204910592ULL
                        ? accept(0,0,2)
                        : rollback(0,0,1);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_one"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_one"),
                fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_root()
    {
        testcase("Test float_root");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_root (int64_t float1, uint32_t n);
                extern int64_t float_one (void);
                extern int64_t float_compare(int64_t, int64_t, uint32_t);
                extern int64_t float_negate(int64_t);
                extern int64_t float_sum(int64_t, int64_t);
                extern int64_t float_mantissa(int64_t);
                extern int64_t float_exponent(int64_t);
                #define ASSERT_EQUAL(x, y)\
                {\
                    int64_t px = (x);\
                    int64_t py = (y);\
                    int64_t mx = float_mantissa(px);\
                    int64_t my = float_mantissa(py);\
                    int32_t diffexp = float_exponent(px) - float_exponent(py);\
                    if (diffexp == 1)\
                        mx *= 10LL;\
                    if (diffexp == -1)\
                        my *= 10LL;\
                    int64_t diffman = mx - my;\
                    if (diffman < 0) diffman *= -1LL;\
                    if (diffexp < 0) diffexp *= -1;\
                    if (diffexp > 1 || diffman > 5000000)\
                        rollback((uint32_t) #x, sizeof(#x), __LINE__);\
                }
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    ASSERT_EQUAL(float_root(float_one(), 2), float_one());

                    // sqrt 9 is 3
                    ASSERT_EQUAL(float_root(6097866696204910592LL, 2), 6091866696204910592LL);

                    // cube root of 1000 is 10
                    ASSERT_EQUAL(float_root(6143909891733356544LL, 3), 6107881094714392576LL);

                    // sqrt of negative is "complex not supported error"
                    if (float_root(1478180677777522688LL, 2) != -39)
                        rollback(0,0,__LINE__);

                    // tenth root of 0 is 0
                    if (float_root(0, 10) != 0)
                        rollback(0,0,__LINE__);

                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_root"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_root"),
                fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_set()
    {
    }

    void
    test_float_sign()
    {
    }

    void
    test_float_sign_set()
    {
    }

    void
    test_float_sto()
    {
    }

    void
    test_float_sto_set()
    {
    }

    void
    test_float_sum()
    {
        testcase("Test float_sum");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook
            hook =
            wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_one (void);
                extern int64_t float_compare(int64_t, int64_t, uint32_t);
                extern int64_t float_negate(int64_t);
                extern int64_t float_sum(int64_t, int64_t);
                extern int64_t float_mantissa(int64_t);
                extern int64_t float_exponent(int64_t);
                #define ASSERT_EQUAL(x, y)\
                {\
                    int64_t px = (x);\
                    int64_t py = (y);\
                    int64_t mx = float_mantissa(px);\
                    int64_t my = float_mantissa(py);\
                    int32_t diffexp = float_exponent(px) - float_exponent(py);\
                    if (diffexp == 1)\
                        mx *= 10LL;\
                    if (diffexp == -1)\
                        my *= 10LL;\
                    int64_t diffman = mx - my;\
                    if (diffman < 0) diffman *= -1LL;\
                    if (diffexp < 0) diffexp *= -1;\
                    if (diffexp > 1 || diffman > 5000000)\
                        rollback((uint32_t) #x, sizeof(#x), __LINE__);\
                }
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // 1 + 1 = 2
                    ASSERT_EQUAL(6090866696204910592LL,
                        float_sum(float_one(), float_one()));

                    // 1 - 1 = 0
                    ASSERT_EQUAL(0,
                        float_sum(float_one(), float_negate(float_one())));

                    // 45678 + 0.345678 = 45678.345678
                    ASSERT_EQUAL(6165492124810638528LL,
                        float_sum(6165492090242838528LL, 6074309077695428608LL));

                    // -151864512641 + 100000000000000000 = 99999848135487359
                    ASSERT_EQUAL(
                        6387097057170171072LL,
                        float_sum(1676857706508234512LL, 6396111470866104320LL));

                    // auto generated random sums
                    ASSERT_EQUAL(
                        float_sum(
                            95785354843184473 /* -5.713362295774553e-77 */,
                            7607324992379065667 /* 5.248821377668419e+84 */),
                        7607324992379065667 /* 5.248821377668419e+84 */);
                    ASSERT_EQUAL(
                        float_sum(
                            1011203427860697296 /* -2.397111329706192e-26 */,
                            7715811566197737722 /* 5.64900413944857e+90 */),
                        7715811566197737722 /* 5.64900413944857e+90 */);
                    ASSERT_EQUAL(
                        float_sum(
                            6507979072644559603 /* 4.781210721563379e+23 */,
                            422214339164556094 /* -7.883173446470462e-59 */),
                        6507979072644559603 /* 4.781210721563379e+23 */);
                    ASSERT_EQUAL(
                        float_sum(
                            129493221419941559 /* -3.392431853567671e-75 */,
                            6742079437952459317 /* 4.694395406197301e+36 */),
                        6742079437952459317 /* 4.694395406197301e+36 */);
                    ASSERT_EQUAL(
                        float_sum(
                            5172806703808250354 /* 2.674331586920946e-51 */,
                            3070396690523275533 /* -7.948943911338253e+88 */),
                        3070396690523275533 /* -7.948943911338253e+88 */);
                    ASSERT_EQUAL(
                        float_sum(
                            2440992231195047997 /* -9.048432414980156e+53 */,
                            4937813945440933271 /* 1.868753842869655e-64 */),
                        2440992231195047996 /* -9.048432414980156e+53 */);
                    ASSERT_EQUAL(
                        float_sum(
                            7351918685453062372 /* 2.0440935844129e+70 */,
                            6489541496844182832 /* 4.358033430668592e+22 */),
                        7351918685453062372 /* 2.0440935844129e+70 */);
                    ASSERT_EQUAL(
                        float_sum(
                            4960621423606196948 /* 6.661833498651348e-63 */,
                            6036716382996689576 /* 0.001892882320224936 */),
                        6036716382996689576 /* 0.001892882320224936 */);
                    ASSERT_EQUAL(
                        float_sum(
                            1342689232407435206 /* -9.62374270576839e-8 */,
                            5629833007898276923 /* 9.340672939897915e-26 */),
                        1342689232407435206 /* -9.62374270576839e-8 */);
                    ASSERT_EQUAL(
                        float_sum(
                            7557687707019793516 /* 9.65473154684222e+81 */,
                            528084028396448719 /* -5.666471621471183e-53 */),
                        7557687707019793516 /* 9.65473154684222e+81 */);
                    ASSERT_EQUAL(
                        float_sum(
                            130151633377050812 /* -4.050843810676924e-75 */,
                            2525286695563827336 /* -3.270904236349576e+58 */),
                        2525286695563827336 /* -3.270904236349576e+58 */);
                    ASSERT_EQUAL(
                        float_sum(
                            5051914485221832639 /* 7.88290256687712e-58 */,
                            7518727241611221951 /* 6.723063157234623e+79 */),
                        7518727241611221951 /* 6.723063157234623e+79 */);
                    ASSERT_EQUAL(
                        float_sum(
                            3014788764095798870 /* -6.384213012307542e+85 */,
                            7425019819707800346 /* 3.087633801222938e+74 */),
                        3014788764095767995 /* -6.384213012276667e+85 */);
                    ASSERT_EQUAL(
                        float_sum(
                            4918950856932792129 /* 1.020063844210497e-65 */,
                            7173510242188034581 /* 3.779635414204949e+60 */),
                        7173510242188034581 /* 3.779635414204949e+60 */);
                    ASSERT_EQUAL(
                        float_sum(
                            20028000442705357 /* -2.013601933223373e-81 */,
                            95248745393457140 /* -5.17675284604722e-77 */),
                        95248946753650462 /* -5.176954206240542e-77 */);
                    ASSERT_EQUAL(
                        float_sum(
                            5516870225060928024 /* 4.46428115944092e-32 */,
                            7357202055584617194 /* 7.327463715967722e+70 */),
                        7357202055584617194 /* 7.327463715967722e+70 */);
                    ASSERT_EQUAL(
                        float_sum(
                            2326103538819088036 /* -2.2461310959121e+47 */,
                            1749360946246242122 /* -1964290826489674 */),
                        2326103538819088036 /* -2.2461310959121e+47 */);
                    ASSERT_EQUAL(
                        float_sum(
                            1738010758208819410 /* -862850129854894.6 */,
                            2224610859005732191 /* -8.83984233944816e+41 */),
                        2224610859005732192 /* -8.83984233944816e+41 */);
                    ASSERT_EQUAL(
                        float_sum(
                            4869534730307487904 /* 5.647132747352224e-68 */,
                            2166841923565712115 /* -5.114102427874035e+38 */),
                        2166841923565712115 /* -5.114102427874035e+38 */);
                    ASSERT_EQUAL(
                        float_sum(
                            1054339559322014937 /* -9.504445772059864e-24 */,
                            1389511416678371338 /* -0.0000240273144825857 */),
                        1389511416678371338 /* -0.0000240273144825857 */);

                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_sum"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)),
                M("test float_sum"),
                fee(XRP(1)));
            env.close();
        }
    }

    void
    test_hook_account()
    {
    }

    void
    test_hook_again()
    {
    }

    void
    test_hook_hash()
    {
    }

    void
    test_hook_namespace()
    {
    }

    void
    test_hook_param()
    {
    }

    void
    test_hook_param_set()
    {
    }

    void
    test_hook_pos()
    {
    }

    void
    test_hook_skip()
    {
    }

    void
    test_ledger_keylet()
    {
    }

    void
    test_ledger_last_hash()
    {
    }

    void
    test_ledger_last_time()
    {
    }

    void
    test_ledger_nonce()
    {
    }

    void
    test_ledger_seq()
    {
    }

    void
    test_meta_slot()
    {
    }

    void
    test_otxn_burden()
    {
    }

    void
    test_otxn_field()
    {
    }

    void
    test_otxn_field_txt()
    {
    }

    void
    test_otxn_generation()
    {
    }

    void
    test_otxn_id()
    {
    }

    void
    test_otxn_slot()
    {
    }

    void
    test_otxn_type()
    {
    }

    void
    test_slot()
    {
    }

    void
    test_slot_clear()
    {
    }

    void
    test_slot_count()
    {
    }

    void
    test_slot_float()
    {
    }

    void
    test_slot_id()
    {
    }

    void
    test_slot_set()
    {
    }

    void
    test_slot_size()
    {
    }

    void
    test_slot_subarray()
    {
    }

    void
    test_slot_subfield()
    {
    }

    void
    test_slot_type()
    {
    }

    void
    test_state()
    {
    }

    void
    test_state_foreign()
    {
    }

    void
    test_state_foreign_set()
    {
    }

    void
    test_state_set()
    {
    }

    void
    test_sto_emplace()
    {
    }

    void
    test_sto_erase()
    {
    }

    void
    test_sto_subarray()
    {
    }

    void
    test_sto_subfield()
    {
    }

    void
    test_sto_validate()
    {
    }

    void
    test_str_compare()
    {
    }

    void
    test_str_concat()
    {
    }

    void
    test_str_find()
    {
    }

    void
    test_str_replace()
    {
    }

    void
    test_trace()
    {
    }

    void
    test_trace_float()
    {
    }

    void
    test_trace_num()
    {
    }

    void
    test_trace_slot()
    {
    }

    void
    test_util_accid()
    {
    }

    void
    test_util_keylet()
    {
    }

    void
    test_util_raddr()
    {
    }

    void
    test_util_sha512h()
    {
    }

    void
    test_util_verify()
    {
    }

    void
    run() override
    {
        //testTicketSetHook();  // RH TODO
        testHooksDisabled();
        testTxStructure();
        testInferHookSetOperation();
        testParams();
        testGrants();

        testDelete();
        testInstall();
        testCreate();

        testUpdate();

        testNSDelete();

        testWasm();
        test_accept();
        test_rollback();

        testGuards();

        test_emit();
        test_etxn_burden();
        test_etxn_details();
        test_etxn_fee_base();
        test_etxn_generation();
        test_etxn_nonce();
        test_etxn_reserve();
        test_fee_base();
        test_float_compare();
        test_float_divide();
        test_float_exponent();
        test_float_exponent();
        test_float_exponent_set();
        test_float_int();
        test_float_invert();
        test_float_log();
        test_float_mantissa();
        test_float_mantissa();
        test_float_mantissa_set();
        test_float_mulratio();
        test_float_multiply();
        test_float_negate();
        test_float_one();
        test_float_root();
        test_float_set();
        test_float_sign();
        test_float_sign_set();
        test_float_sto();
        test_float_sto_set();
        test_float_sum();
        test_hook_account();
        test_hook_again();
        test_hook_hash();
        test_hook_namespace();
        test_hook_param();
        test_hook_param_set();
        test_hook_pos();
        test_hook_skip();
        test_ledger_keylet();
        test_ledger_last_hash();
        test_ledger_last_time();
        test_ledger_nonce();
        test_ledger_seq();
        test_meta_slot();
        test_otxn_burden();
        test_otxn_field();
        test_otxn_field_txt();
        test_otxn_generation();
        test_otxn_id();
        test_otxn_slot();
        test_otxn_type();
        test_slot();
        test_slot_clear();
        test_slot_count();
        test_slot_float();
        test_slot_id();
        test_slot_set();
        test_slot_size();
        test_slot_subarray();
        test_slot_subfield();
        test_slot_type();
        test_state();
        test_state_foreign();
        test_state_foreign_set();
        test_state_set();
        test_sto_emplace();
        test_sto_erase();
        test_sto_subarray();
        test_sto_subfield();
        test_sto_validate();
        test_str_compare();
        test_str_concat();
        test_str_find();
        test_str_replace();
        test_trace();
        test_trace_float();
        test_trace_num();
        test_trace_slot();
        test_util_accid();
        test_util_keylet();
        test_util_raddr();
        test_util_sha512h();
        test_util_verify();
    }

private:
#define HASH_WASM(x)\
    uint256 const x##_hash = ripple::sha512Half_s(ripple::Slice(x##_wasm.data(), x##_wasm.size()));\
    std::string const x##_hash_str = to_string(x##_hash);\
    Keylet const x##_keylet = keylet::hookDefinition(x##_hash);

    TestHook
    accept_wasm =   // WASM: 0
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

    HASH_WASM(accept);

    TestHook
    rollback_wasm = // WASM: 1
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

    HASH_WASM(rollback);

    TestHook
    noguard_wasm =  // WASM: 2
    wasm[
        R"[test.hook](
            (module
              (type (;0;) (func (param i32 i32 i64) (result i64)))
              (type (;1;) (func (param i32) (result i64)))
              (import "env" "accept" (func (;0;) (type 0)))
              (func (;1;) (type 1) (param i32) (result i64)
                i32.const 0
                i32.const 0
                i64.const 0
                call 0)
              (memory (;0;) 2)
              (export "memory" (memory 0))
              (export "hook" (func 1)))
        )[test.hook]"
    ];

    TestHook
    illegalfunc_wasm = // WASM: 3
    wasm[
        R"[test.hook](
            (module
              (type (;0;) (func (param i32 i32) (result i32)))
              (type (;1;) (func (param i32 i32 i64) (result i64)))
              (type (;2;) (func))
              (type (;3;) (func (param i32) (result i64)))
              (import "env" "_g" (func (;0;) (type 0)))
              (import "env" "accept" (func (;1;) (type 1)))
              (func (;2;) (type 3) (param i32) (result i64)
                i32.const 1
                i32.const 1
                call 0
                drop
                i32.const 0
                i32.const 0
                i64.const 0
                call 1)
              (func (;3;) (type 2)
                i32.const 1
                i32.const 1
                call 0
                drop)
              (memory (;0;) 2)
              (global (;0;) (mut i32) (i32.const 66560))
              (global (;1;) i32 (i32.const 1024))
              (global (;2;) i32 (i32.const 1024))
              (global (;3;) i32 (i32.const 66560))
              (global (;4;) i32 (i32.const 1024))
              (export "memory" (memory 0))
              (export "hook" (func 2))
              (export "bad_func" (func 3)))
        )[test.hook]"
    ];

    TestHook
    long_wasm = // WASM: 4
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
                _g(1,1);
                char ret[] = M_REPEAT_1000("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890123");
                return accept(SBUF(ret), 0);
            }
        )[test.hook]"
    ];

    TestHook
    makestate_wasm = // WASM: 5
    wasm[
        R"[test.hook](
            #include <stdint.h>
            extern int32_t _g           (uint32_t id, uint32_t maxiter);
            extern int64_t accept       (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t state_set    (uint32_t read_ptr, uint32_t read_len, uint32_t kread_ptr, uint32_t kread_len);
            #define SBUF(x) x, sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t test_key[] = "key";
                uint8_t test_value[] = "value";
                return accept(0,0, state_set(SBUF(test_value), SBUF(test_key)));
            }
        )[test.hook]"
    ];

    HASH_WASM(makestate);

    // this is just used as a second small hook with a unique hash
    TestHook
    accept2_wasm =   // WASM: 6
    wasm[
        R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                return accept(0,0,2);
            }
        )[test.hook]"
    ];

    HASH_WASM(accept2);

};
BEAST_DEFINE_TESTSUITE(SetHook, tx, ripple);
}  // namespace test
}  // namespace ripple

