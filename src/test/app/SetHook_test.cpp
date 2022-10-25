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
public:

    // This is a large fee, large enough that we can set most small test hooks without running into fee issues
    // we only want to test fee code specifically in fee unit tests, the rest of the time we want to ignore it.
    #define HSFEE fee(1'000'000)
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

        auto overrideFlag = [](Json::Value& jv)
        {
            jv[jss::Flags] = hsfOVERRIDE;
        };
        
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
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t hook_account (uint32_t, uint32_t);
                int64_t hook(uint32_t reserved)
                {
                    uint8_t acc[20];
                    for (int i = 0; GUARD(10), i < 10; ++i)
                    {
                        for (int j = 0; GUARD(2), j < 2; ++j)
                        {
                            for (int k = 0; GUARD(5), k < 5; ++k)
                                hook_account(acc, 20);
                            for (int k = 0; GUARD(5), k < 5; ++k)
                                hook_account(acc, 20);
                        }

                        for (int k = 0; GUARD(5), k < 5; ++k)
                            hook_account(acc, 20);
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
    }

    void
    test_float_divide()
    {
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
    }

    void
    test_float_invert()
    {
    }

    void
    test_float_log()
    {
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
    }

    void
    test_float_one()
    {
    }

    void
    test_float_root()
    {
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
