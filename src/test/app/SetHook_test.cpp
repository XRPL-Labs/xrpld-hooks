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
#include <ripple/app/tx/impl/SetHook.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/jss.h>
#include <test/app/SetHook_wasm.h>
#include <test/jtx.h>
#include <test/jtx/hook.h>
#include <unordered_map>

namespace ripple {

namespace test {

#define DEBUG_TESTS 1

using TestHook = std::vector<uint8_t> const&;

class JSSHasher
{
public:
    size_t
    operator()(const Json::StaticString& n) const
    {
        return std::hash<std::string_view>{}(n.c_str());
    }
};

class JSSEq
{
public:
    bool
    operator()(const Json::StaticString& a, const Json::StaticString& b) const
    {
        return a == b;
    }
};

using JSSMap =
    std::unordered_map<Json::StaticString, Json::Value, JSSHasher, JSSEq>;

// Identical to BEAST_EXPECT except it returns from the function
// if the condition isn't met (and would otherwise therefore cause a crash)
#define BEAST_REQUIRE(x)     \
    {                        \
        BEAST_EXPECT(!!(x)); \
        if (!(x))            \
            return;          \
    }

#define HASH_WASM(x)                                                           \
    uint256 const x##_hash =                                                   \
        ripple::sha512Half_s(ripple::Slice(x##_wasm.data(), x##_wasm.size())); \
    std::string const x##_hash_str = to_string(x##_hash);                      \
    Keylet const x##_keylet = keylet::hookDefinition(x##_hash);

class SetHook_test : public beast::unit_test::suite
{
private:
    // helper
    void static overrideFlag(Json::Value& jv)
    {
        jv[jss::Flags] = hsfOVERRIDE;
    }

public:
// This is a large fee, large enough that we can set most small test hooks
// without running into fee issues we only want to test fee code specifically in
// fee unit tests, the rest of the time we want to ignore it.
#define HSFEE fee(100'000'000)
#define M(m) memo(m, "", "")
    void
    testHooksDisabled()
    {
        testcase("Check for disabled amendment");
        using namespace jtx;
        Env env{*this, supported_amendments() - featureHooks};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);

        // RH TODO: does it matter that passing malformed txn here gives back
        // temMALFORMED (and not disabled)?
        env(ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0),
            M("Hooks Disabled"),
            HSFEE,
            ter(temDISABLED));
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
            HSFEE,
            ter(temMALFORMED));

        env(ripple::test::jtx::hook(alice, {{}}, 0),
            M("Must have a non-empty hooks field"),
            HSFEE,
            ter(temMALFORMED));

        env(ripple::test::jtx::hook(
                alice,
                {{hso(accept_wasm),
                  hso(accept_wasm),
                  hso(accept_wasm),
                  hso(accept_wasm),
                  hso(accept_wasm)}},
                0),
            M("Must have fewer than 5 entries"),
            HSFEE,
            ter(temMALFORMED));

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
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }
    }

    void
    testGrants()
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
            Json::Value grants{Json::arrayValue};
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
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // check wrong inner type
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value grants{Json::arrayValue};
            grants[0U] = Json::Value{};
            grants[0U][jss::Memo] = Json::Value{};
            grants[0U][jss::Memo][jss::MemoFormat] =
                strHex(std::string(12, 'a'));
            grants[0U][jss::Memo][jss::MemoData] = strHex(std::string(12, 'a'));
            iv[jss::HookGrants] = grants;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO grant array can only contain HookGrant objects"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }
    }

    void
    testParams()
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
            Json::Value params{Json::arrayValue};
            for (uint32_t i = 0; i < 17; ++i)
            {
                Json::Value pv;
                Json::Value piv;
                piv[jss::HookParameterName] =
                    strHex("param" + std::to_string(i));
                piv[jss::HookParameterValue] =
                    strHex("value" + std::to_string(i));
                pv[jss::HookParameter] = piv;
                params[i] = pv;
            }
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must not include more than 16 parameters"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // check repeat parameters
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params{Json::arrayValue};
            for (uint32_t i = 0; i < 2; ++i)
            {
                params[i] = Json::Value{};
                params[i][jss::HookParameter] = Json::Value{};
                params[i][jss::HookParameter][jss::HookParameterName] =
                    strHex(std::string{"param"});
            }
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must not repeat parameter names"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // check too long parameter name
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params{Json::arrayValue};
            params[0U] = Json::Value{};
            params[0U][jss::HookParameter] = Json::Value{};
            params[0U][jss::HookParameter][jss::HookParameterName] =
                strHex(std::string(33, 'a'));
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must must not contain parameter names longer than 32 "
                  "bytes"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // check too long parameter value
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params{Json::arrayValue};
            params[0U] = Json::Value{};
            params[0U][jss::HookParameter] = Json::Value{};
            params[0U][jss::HookParameter][jss::HookParameterName] =
                strHex(std::string(32, 'a'));
            params[0U][jss::HookParameter][jss::HookParameterValue] =
                strHex(std::string(257, 'a'));
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must must not contain parameter values longer than 256 "
                  "bytes"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // wrong object type
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value params{Json::arrayValue};
            params[0U] = Json::Value{};
            params[0U][jss::Memo] = Json::Value{};
            params[0U][jss::Memo][jss::MemoFormat] =
                strHex(std::string(12, 'a'));
            params[0U][jss::Memo][jss::MemoData] = strHex(std::string(12, 'a'));
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO parameter array can only contain HookParameter objects"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }
    }

    void
    testInstall()
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
            env(ripple::test::jtx::hook(
                    bob, {{hso(accept_wasm), hso(rollback_wasm)}}, 0),
                M("First set = tesSUCCESS"),
                HSFEE,
                ter(tesSUCCESS));
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
            iv[jss::HookApiVersion] = 0U;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation cannot set apiversion"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // can't set non-existent hook
        {
            Json::Value iv;
            iv[jss::HookHash] =
                "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBE"
                "EF";
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation cannot set non existent hook hash"),
                HSFEE,
                ter(terNO_HOOK));
            env.close();
        }

        // can set extant hook
        {
            Json::Value iv;
            iv[jss::HookHash] = accept_hash_str;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation can set extant hook hash"),
                HSFEE,
                ter(tesSUCCESS));
            env.close();
        }

        // can't set extant hook over other hook without override flag
        {
            Json::Value iv;
            iv[jss::HookHash] = rollback_hash_str;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation can set extant hook hash"),
                HSFEE,
                ter(tecREQUIRES_FLAG));
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
                HSFEE,
                ter(tesSUCCESS));
            env.close();
        }
    }

    void
    testDelete()
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
                HSFEE,
                ter(temMALFORMED));
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
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // grants, parameters, hookon, hookapiversion, hooknamespace keys must
        // be absent
        for (auto const& [key, value] : JSSMap{
                 {jss::HookGrants, Json::arrayValue},
                 {jss::HookParameters, Json::arrayValue},
                 {jss::HookOn, "0"},
                 {jss::HookApiVersion, "0"},
                 {jss::HookNamespace, to_string(uint256{beast::zero})}})
        {
            Json::Value iv;
            iv[jss::CreateCode] = "";
            iv[key] = value;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook DELETE operation cannot include: grants, params, "
                  "hookon, apiversion, namespace"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // create and delete single hook
        {
            {
                Json::Value jv =
                    ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0);
                env(jv, M("Normal accept create"), HSFEE, ter(tesSUCCESS));
                env.close();
            }

            BEAST_REQUIRE(env.le(accept_keylet));

            Json::Value iv;
            iv[jss::CreateCode] = "";
            iv[jss::Flags] = hsfOVERRIDE;
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv, M("Normal hook DELETE"), HSFEE);
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
                Json::Value jv = ripple::test::jtx::hook(
                    alice,
                    {{hso(accept_wasm),
                      hso(makestate_wasm),
                      hso(rollback_wasm),
                      hso(accept2_wasm)}},
                    0);
                env(jv, M("Create four"), HSFEE, ter(tesSUCCESS));
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

                env(jv, M("Normal hooki DELETE (third pos)"), HSFEE);
                env.close();

                // check the hook definitions are consistent with reference
                // count dropping to zero on the third
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
                BEAST_EXPECT(
                    hooks[1].getFieldH256(sfHookHash) == makestate_hash);
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

                // check the hook definitions are consistent with reference
                // count dropping to zero on the third
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

    void
    testNSDelete()
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

        for (auto const& [key, value] : JSSMap{
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
                M("Hook NSDELETE operation cannot include: grants, params, "
                  "hookon, apiversion"),
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        auto const key = uint256::fromVoid(
            (std::array<uint8_t, 32>{
                 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                 0x00U, 0x00U, 0x00U, 0x00U, 'k',   'e',   'y',   0x00U})
                .data());

        auto const ns = uint256::fromVoid(
            (std::array<uint8_t, 32>{
                 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU})
                .data());

        auto const stateKeylet =
            keylet::hookState(Account("alice").id(), key, ns);

        // create a namespace
        std::string ns_str =
            "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE";
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
            env(jv, M("Normal NSDELETE operation"), HSFEE, ter(tesSUCCESS));
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
            auto const dirKeylet =
                keylet::hookStateDir(Account("alice").id(), ns);
            BEAST_EXPECT(!env.le(dirKeylet));

            // ensure the state object is gone
            BEAST_EXPECT(!env.le(stateKeylet));
        }
    }

    void
    testCreate()
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
                HSFEE,
                ter(tesSUCCESS));

            env(ripple::test::jtx::hook(bob, {{hso(accept_wasm)}}, 0),
                M("Second set = tecREQUIRES_FLAG"),
                HSFEE,
                ter(tecREQUIRES_FLAG));
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
                HSFEE,
                ter(temMALFORMED));
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
                HSFEE,
                ter(temMALFORMED));
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
                HSFEE,
                ter(temMALFORMED));
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
                HSFEE,
                ter(temMALFORMED));
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
                HSFEE,
                ter(temMALFORMED));
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
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // correctly formed
        {
            Json::Value jv =
                ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0);
            env(jv, M("Normal accept"), HSFEE, ter(tesSUCCESS));
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
            auto const wasm_hash =
                sha512Half_s(ripple::Slice(wasm.data(), wasm.size()));
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
                HSFEE,
                ter(tesSUCCESS));
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
            ripple::Slice(rollback_wasm.data(), rollback_wasm.size()));

        // test override
        {
            Json::Value jv =
                ripple::test::jtx::hook(alice, {{hso(rollback_wasm)}}, 0);
            jv[jss::Hooks][0U][jss::Hook][jss::Flags] = hsfOVERRIDE;
            env(jv, M("Rollback override"), HSFEE, ter(tesSUCCESS));
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
            auto const wasm_hash =
                sha512Half_s(ripple::Slice(wasm.data(), wasm.size()));
            BEAST_EXPECT(wasm_hash == rollback_hash);

            // check if the reference count was correctly incremented
            BEAST_EXPECT(rollback_def->isFieldPresent(sfReferenceCount));
            BEAST_EXPECT(rollback_def->getFieldU64(sfReferenceCount) == 1ULL);

            // check if the reference count was correctly decremented
            BEAST_EXPECT(accept_def->isFieldPresent(sfReferenceCount));
            BEAST_EXPECT(accept_def->getFieldU64(sfReferenceCount) == 2ULL);
        }
    }

    void
    testUpdate()
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
            iv[jss::HookParameters][0U][jss::HookParameter]
              [jss::HookParameterName] = "AAAAAAAAAAAA";
            iv[jss::HookParameters][0U][jss::HookParameter]
              [jss::HookParameterValue] = "BBBBBB";

            iv[jss::HookParameters][1U] = Json::Value{};
            iv[jss::HookParameters][1U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][1U][jss::HookParameter]
              [jss::HookParameterName] = "CAFE";
            iv[jss::HookParameters][1U][jss::HookParameter]
              [jss::HookParameterValue] = "FACADE";

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv, M("Create accept"), HSFEE, ter(tesSUCCESS));
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
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // must not specify NSDELETE unless also Namespace
        {
            Json::Value iv;
            iv[jss::Flags] = hsfNSDELETE;
            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;

            env(jv,
                M("NSDELETE flag not allowed on update unless HookNamespace "
                  "also present"),
                HSFEE,
                ter(temMALFORMED));
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
                HSFEE,
                ter(temMALFORMED));
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

            for (auto const& [key, value] : JSSMap{
                     {jss::HookOn, "1"},
                     {jss::HookNamespace,
                      "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE"
                      "CAFECAFE"},
                     {jss::HookParameters, params},
                     {jss::HookGrants, grants}})
            {
                Json::Value iv;
                iv[key] = value;
                jv[jss::Hooks][0U] = Json::Value{};
                jv[jss::Hooks][0U][jss::Hook] = iv;

                env(jv, M("Normal update"), HSFEE, ter(tesSUCCESS));
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

            auto const ns = uint256::fromVoid(
                (std::array<uint8_t, 32>{
                     0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                     0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                     0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU,
                     0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU, 0xCAU, 0xFEU})
                    .data());
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
            BEAST_REQUIRE(pv[0] == 0xBAU && pv[1] == 0xBEU);

            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookGrants));
            const auto& g = hooks[0].getFieldArray(sfHookGrants);
            BEAST_REQUIRE(g.size() == 1);
            BEAST_REQUIRE(g[0].isFieldPresent(sfHookHash));
            BEAST_REQUIRE(g[0].getFieldH256(sfHookHash) == accept_hash);
        }

        // reset hookon and namespace to defaults
        {
            for (auto const& [key, value] : JSSMap{
                     {jss::HookOn, "0"},
                     {jss::HookNamespace, to_string(uint256{beast::zero})}})
            {
                Json::Value iv;
                iv[key] = value;
                jv[jss::Hooks][0U] = Json::Value{};
                jv[jss::Hooks][0U][jss::Hook] = iv;

                env(jv, M("Reset to default"), HSFEE, ter(tesSUCCESS));
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

            // ensure the two fields are now absent (because they were reset to
            // the defaults on the hook def)
            BEAST_EXPECT(!hooks[0].isFieldPresent(sfHookOn));
            BEAST_EXPECT(!hooks[0].isFieldPresent(sfHookNamespace));
        }

        // add three additional parameters
        std::map<ripple::Blob, ripple::Blob> params{
            {{0xFEU, 0xEDU, 0xFAU, 0xCEU}, {0xF0U, 0x0DU}},
            {{0xA0U}, {0xB0U}},
            {{0xCAU, 0xFEU}, {0xBAU, 0xBEU}},
            {{0xAAU}, {0xBBU, 0xCCU}}};
        {
            Json::Value iv;
            iv[jss::HookParameters] = Json::Value{Json::arrayValue};
            iv[jss::HookParameters][0U] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter]
              [jss::HookParameterName] = "FEEDFACE";
            iv[jss::HookParameters][0U][jss::HookParameter]
              [jss::HookParameterValue] = "F00D";

            iv[jss::HookParameters][1U] = Json::Value{};
            iv[jss::HookParameters][1U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][1U][jss::HookParameter]
              [jss::HookParameterName] = "A0";
            iv[jss::HookParameters][1U][jss::HookParameter]
              [jss::HookParameterValue] = "B0";

            iv[jss::HookParameters][2U] = Json::Value{};
            iv[jss::HookParameters][2U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][2U][jss::HookParameter]
              [jss::HookParameterName] = "AA";
            iv[jss::HookParameters][2U][jss::HookParameter]
              [jss::HookParameterValue] = "BBCC";

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv, M("Add three parameters"), HSFEE, ter(tesSUCCESS));
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
            iv[jss::HookParameters][0U][jss::HookParameter]
              [jss::HookParameterName] = "CAFE";

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Reset cafe param to default using Absent Value"),
                HSFEE,
                ter(tesSUCCESS));
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

            // and that they still have the expected values and that there are
            // no duplicates
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

        // now re-add CAFE parameter but this time as an explicit blank (Empty
        // value)
        {
            Json::Value iv;
            iv[jss::HookParameters] = Json::Value{Json::arrayValue};
            iv[jss::HookParameters][0U] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter] = Json::Value{};
            iv[jss::HookParameters][0U][jss::HookParameter]
              [jss::HookParameterName] = "CAFE";
            iv[jss::HookParameters][0U][jss::HookParameter]
              [jss::HookParameterValue] = "";

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Set cafe param to blank using Empty Value"),
                HSFEE,
                ter(tesSUCCESS));
            env.close();

            // ensure hook still exists
            auto const hook = env.le(keylet::hook(Account("alice").id()));
            BEAST_REQUIRE(hook);
            BEAST_REQUIRE(hook->isFieldPresent(sfHooks));
            auto const& hooks = hook->getFieldArray(sfHooks);
            BEAST_EXPECT(hooks.size() == 1);
            BEAST_EXPECT(hooks[0].isFieldPresent(sfHookHash));
            BEAST_EXPECT(hooks[0].getFieldH256(sfHookHash) == accept_hash);

            params[Blob{0xCAU, 0xFEU}] = Blob{};

            // check there right number of parameters exist
            BEAST_REQUIRE(hooks[0].isFieldPresent(sfHookParameters));
            const auto& p = hooks[0].getFieldArray(sfHookParameters);
            BEAST_REQUIRE(p.size() == params.size());

            // and that they still have the expected values and that there are
            // no duplicates
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

        // try to delete all parameters (reset to defaults) using EMA (Empty
        // Parameters Array)
        {
            Json::Value iv;
            iv[jss::HookParameters] = Json::Value{Json::arrayValue};

            jv[jss::Hooks][0U] = Json::Value{};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv, M("Unset all params on hook"), HSFEE, ter(tesSUCCESS));
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

            for (auto const& [key, value] : JSSMap{
                     {jss::HookOn, "1"},
                     {jss::HookNamespace,
                      "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE"
                      "CAFECAFE"},
                     {jss::HookParameters, params},
                     {jss::HookGrants, grants}})
            {
                Json::Value iv;
                iv[key] = value;
                jv[jss::Hooks][0U] = Json::Value{};
                jv[jss::Hooks][0U][jss::Hook] = Json::Value{};
                jv[jss::Hooks][1U] = Json::Value{};
                jv[jss::Hooks][1U][jss::Hook] = iv;

                env(jv,
                    M("Invalid update on non existent hook"),
                    HSFEE,
                    ter(tecNO_ENTRY));
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
                    HSFEE,
                    ter(tesSUCCESS));
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

            env(jv, M("Add grants"), HSFEE);
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

                BEAST_EXPECT(
                    grants[0].getFieldH256(sfHookHash) == rollback_hash);
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

            env(jv, M("update grants"), HSFEE);
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
                BEAST_EXPECT(
                    grants[0].getFieldH256(sfHookHash) == makestate_hash);
            }
        }

        // use an empty grants array to reset the grants
        {
            jv[jss::Hooks][0U] = Json::objectValue;
            jv[jss::Hooks][0U][jss::Hook] = Json::objectValue;
            jv[jss::Hooks][1U] = Json::Value{};
            jv[jss::Hooks][1U][jss::Hook] = Json::Value{};
            jv[jss::Hooks][1U][jss::Hook][jss::HookGrants] = Json::arrayValue;

            env(jv, M("clear grants"), HSFEE);
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

    void
    testInferHookSetOperation()
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
            hso.setFieldH256(
                sfHookHash, uint256{beast::zero});  // all zeros hook hash
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoINSTALL);
        }

        // hsoNSDELETE
        {
            STObject hso{sfHook};
            hso.setFieldH256(
                sfHookNamespace, uint256{beast::zero});  // all zeros hook hash
            hso.setFieldU32(sfFlags, hsfNSDELETE);
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoNSDELETE);
        }

        // hsoUPDATE
        {
            STObject hso{sfHook};
            hso.setFieldU64(sfHookOn, 1LLU);
            BEAST_EXPECT(SetHook::inferOperation(hso) == hsoUPDATE);
        }

        // hsoINVALID
        {
            STObject hso{sfHook};
            hso.setFieldVL(sfCreateCode, {1});  // non-empty create code
            hso.setFieldH256(
                sfHookHash, uint256{beast::zero});  // all zeros hook hash
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
            HSFEE,
            ter(temMALFORMED));

        env(ripple::test::jtx::hook(alice, {{hso(illegalfunc_wasm)}}, 0),
            M("Must only contain hook and cbak"),
            HSFEE,
            ter(temMALFORMED));
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

        env(pay(bob, alice, XRP(1)), M("Test Accept Hook"), fee(XRP(1)));
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
            fee(XRP(1)),
            ter(tecHOOK_REJECTED));
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
            TestHook hook = wasm[R"[test.hook](
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
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }

        // same loop again but with a guard call
        {
            TestHook hook = wasm[R"[test.hook](
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
            TestHook hook = wasm[R"[test.hook](
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

            env(pay(bob, alice, XRP(1)), M("Test Loop 2"), fee(XRP(1)));
            env.close();
        }

        // complex looping, c
        {
            TestHook hook = wasm[R"[test.hook](
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

            env(pay(bob, alice, XRP(1)), M("Test Loop 3"), fee(XRP(1)));
            env.close();
        }

        // complex looping missing a guard
        {
            TestHook hook = wasm[R"[test.hook](
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
                HSFEE,
                ter(temMALFORMED));
            env.close();
        }
    }

    void
    test_emit()
    {
        testcase("Test float_emit");
        using namespace jtx;
        Env env{*this, envconfig(), supported_amendments(), nullptr, 
            beast::severities::kWarning
//            beast::severities::kTrace
        };

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);
            
        TestHook hook = wasm[R"[test.hook](
        #include <stdint.h>
        extern int32_t _g(uint32_t, uint32_t);
        extern int64_t accept (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
        extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
        extern int64_t emit (uint32_t, uint32_t, uint32_t, uint32_t);
        extern int64_t etxn_reserve(uint32_t);
        extern int64_t otxn_param(uint32_t, uint32_t, uint32_t, uint32_t);
        extern int64_t hook_account(uint32_t, uint32_t);
        extern int64_t otxn_field (
            uint32_t write_ptr,
            uint32_t write_len,
            uint32_t field_id
        );
        #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
        #define OUT_OF_BOUNDS (-1)
        #define ttPAYMENT 0
        #define tfCANONICAL 0x80000000UL
        #define amAMOUNT 1U
        #define amFEE 8U
        #define atACCOUNT 1U
        #define DOESNT_EXIST (-5)
        #define atDESTINATION 3U
        #define SBUF(x) (uint32_t)x,sizeof(x)

        #define PREREQUISITE_NOT_MET -9
        #define ENCODE_DROPS_SIZE 9
        #define ENCODE_DROPS(buf_out, drops, amount_type ) \
            {\
                uint8_t uat = amount_type; \
                uint64_t udrops = drops; \
                buf_out[0] = 0x60U +(uat & 0x0FU ); \
                buf_out[1] = 0b01000000 + (( udrops >> 56 ) & 0b00111111 ); \
                buf_out[2] = (udrops >> 48) & 0xFFU; \
                buf_out[3] = (udrops >> 40) & 0xFFU; \
                buf_out[4] = (udrops >> 32) & 0xFFU; \
                buf_out[5] = (udrops >> 24) & 0xFFU; \
                buf_out[6] = (udrops >> 16) & 0xFFU; \
                buf_out[7] = (udrops >>  8) & 0xFFU; \
                buf_out[8] = (udrops >>  0) & 0xFFU; \
                buf_out += ENCODE_DROPS_SIZE; \
            }

        #define _06_XX_ENCODE_DROPS(buf_out, drops, amount_type )\
            ENCODE_DROPS(buf_out, drops, amount_type );

        #define ENCODE_DROPS_AMOUNT(buf_out, drops )\
            ENCODE_DROPS(buf_out, drops, amAMOUNT );
        #define _06_01_ENCODE_DROPS_AMOUNT(buf_out, drops )\
            ENCODE_DROPS_AMOUNT(buf_out, drops );

        #define ENCODE_DROPS_FEE(buf_out, drops )\
            ENCODE_DROPS(buf_out, drops, amFEE );
        #define _06_08_ENCODE_DROPS_FEE(buf_out, drops )\
            ENCODE_DROPS_FEE(buf_out, drops );

        #define ENCODE_TT_SIZE 3
        #define ENCODE_TT(buf_out, tt )\
            {\
                uint8_t utt = tt;\
                buf_out[0] = 0x12U;\
                buf_out[1] =(utt >> 8 ) & 0xFFU;\
                buf_out[2] =(utt >> 0 ) & 0xFFU;\
                buf_out += ENCODE_TT_SIZE; \
            }
        #define _01_02_ENCODE_TT(buf_out, tt)\
            ENCODE_TT(buf_out, tt);


        #define ENCODE_ACCOUNT_SIZE 22
        #define ENCODE_ACCOUNT(buf_out, account_id, account_type)\
            {\
                uint8_t uat = account_type;\
                buf_out[0] = 0x80U + uat;\
                buf_out[1] = 0x14U;\
                *(uint64_t*)(buf_out +  2) = *(uint64_t*)(account_id +  0);\
                *(uint64_t*)(buf_out + 10) = *(uint64_t*)(account_id +  8);\
                *(uint32_t*)(buf_out + 18) = *(uint32_t*)(account_id + 16);\
                buf_out += ENCODE_ACCOUNT_SIZE;\
            }
        #define _08_XX_ENCODE_ACCOUNT(buf_out, account_id, account_type)\
            ENCODE_ACCOUNT(buf_out, account_id, account_type);

        #define ENCODE_ACCOUNT_SRC_SIZE 22
        #define ENCODE_ACCOUNT_SRC(buf_out, account_id)\
            ENCODE_ACCOUNT(buf_out, account_id, atACCOUNT);
        #define _08_01_ENCODE_ACCOUNT_SRC(buf_out, account_id)\
            ENCODE_ACCOUNT_SRC(buf_out, account_id);

        #define ENCODE_ACCOUNT_DST_SIZE 22
        #define ENCODE_ACCOUNT_DST(buf_out, account_id)\
            ENCODE_ACCOUNT(buf_out, account_id, atDESTINATION);
        #define _08_03_ENCODE_ACCOUNT_DST(buf_out, account_id)\
            ENCODE_ACCOUNT_DST(buf_out, account_id);

        #define ENCODE_ACCOUNT_OWNER_SIZE 22
        #define ENCODE_ACCOUNT_OWNER(buf_out, account_id) \
            ENCODE_ACCOUNT(buf_out, account_id, atOWNER);
        #define _08_02_ENCODE_ACCOUNT_OWNER(buf_out, account_id) \
            ENCODE_ACCOUNT_OWNER(buf_out, account_id);

        #define ENCODE_UINT32_COMMON_SIZE 5U
        #define ENCODE_UINT32_COMMON(buf_out, i, field)\
            {\
                uint32_t ui = i; \
                uint8_t uf = field; \
                buf_out[0] = 0x20U +(uf & 0x0FU); \
                buf_out[1] =(ui >> 24 ) & 0xFFU; \
                buf_out[2] =(ui >> 16 ) & 0xFFU; \
                buf_out[3] =(ui >>  8 ) & 0xFFU; \
                buf_out[4] =(ui >>  0 ) & 0xFFU; \
                buf_out += ENCODE_UINT32_COMMON_SIZE; \
            }
        #define _02_XX_ENCODE_UINT32_COMMON(buf_out, i, field)\
            ENCODE_UINT32_COMMON(buf_out, i, field)\

        #define ENCODE_UINT32_UNCOMMON_SIZE 6U
        #define ENCODE_UINT32_UNCOMMON(buf_out, i, field)\
            {\
                uint32_t ui = i; \
                uint8_t uf = field; \
                buf_out[0] = 0x20U; \
                buf_out[1] = uf; \
                buf_out[2] =(ui >> 24 ) & 0xFFU; \
                buf_out[3] =(ui >> 16 ) & 0xFFU; \
                buf_out[4] =(ui >>  8 ) & 0xFFU; \
                buf_out[5] =(ui >>  0 ) & 0xFFU; \
                buf_out += ENCODE_UINT32_UNCOMMON_SIZE; \
            }
        #define _02_XX_ENCODE_UINT32_UNCOMMON(buf_out, i, field)\
            ENCODE_UINT32_UNCOMMON(buf_out, i, field)\

        #define ENCODE_LLS_SIZE 6U
        #define ENCODE_LLS(buf_out, lls )\
            ENCODE_UINT32_UNCOMMON(buf_out, lls, 0x1B );
        #define _02_27_ENCODE_LLS(buf_out, lls )\
            ENCODE_LLS(buf_out, lls );

        #define ENCODE_FLS_SIZE 6U
        #define ENCODE_FLS(buf_out, fls )\
            ENCODE_UINT32_UNCOMMON(buf_out, fls, 0x1A );
        #define _02_26_ENCODE_FLS(buf_out, fls )\
            ENCODE_FLS(buf_out, fls );

        #define ENCODE_TAG_SRC_SIZE 5
        #define ENCODE_TAG_SRC(buf_out, tag )\
            ENCODE_UINT32_COMMON(buf_out, tag, 0x3U );
        #define _02_03_ENCODE_TAG_SRC(buf_out, tag )\
            ENCODE_TAG_SRC(buf_out, tag );

        #define ENCODE_TAG_DST_SIZE 5
        #define ENCODE_TAG_DST(buf_out, tag )\
            ENCODE_UINT32_COMMON(buf_out, tag, 0xEU );
        #define _02_14_ENCODE_TAG_DST(buf_out, tag )\
            ENCODE_TAG_DST(buf_out, tag );

        #define ENCODE_SEQUENCE_SIZE 5
        #define ENCODE_SEQUENCE(buf_out, sequence )\
            ENCODE_UINT32_COMMON(buf_out, sequence, 0x4U );
        #define _02_04_ENCODE_SEQUENCE(buf_out, sequence )\
            ENCODE_SEQUENCE(buf_out, sequence );

        #define ENCODE_FLAGS_SIZE 5
        #define ENCODE_FLAGS(buf_out, tag )\
            ENCODE_UINT32_COMMON(buf_out, tag, 0x2U );
        #define _02_02_ENCODE_FLAGS(buf_out, tag )\
            ENCODE_FLAGS(buf_out, tag );

        #define ENCODE_SIGNING_PUBKEY_SIZE 35
        #define ENCODE_SIGNING_PUBKEY(buf_out, pkey )\
            {\
                buf_out[0] = 0x73U;\
                buf_out[1] = 0x21U;\
                *(uint64_t*)(buf_out +  2) = *(uint64_t*)(pkey +  0);\
                *(uint64_t*)(buf_out + 10) = *(uint64_t*)(pkey +  8);\
                *(uint64_t*)(buf_out + 18) = *(uint64_t*)(pkey + 16);\
                *(uint64_t*)(buf_out + 26) = *(uint64_t*)(pkey + 24);\
                buf[34] = pkey[32];\
                buf_out += ENCODE_SIGNING_PUBKEY_SIZE;\
            }

        #define _07_03_ENCODE_SIGNING_PUBKEY(buf_out, pkey )\
            ENCODE_SIGNING_PUBKEY(buf_out, pkey );

        #define ENCODE_SIGNING_PUBKEY_NULL_SIZE 35
        #define ENCODE_SIGNING_PUBKEY_NULL(buf_out )\
            {\
                buf_out[0] = 0x73U;\
                buf_out[1] = 0x21U;\
                *(uint64_t*)(buf_out+2) = 0;\
                *(uint64_t*)(buf_out+10) = 0;\
                *(uint64_t*)(buf_out+18) = 0;\
                *(uint64_t*)(buf_out+25) = 0;\
                buf_out += ENCODE_SIGNING_PUBKEY_NULL_SIZE;\
            }

        #define _07_03_ENCODE_SIGNING_PUBKEY_NULL(buf_out )\
            ENCODE_SIGNING_PUBKEY_NULL(buf_out );

        extern int64_t etxn_fee_base (
            uint32_t read_ptr,
          	uint32_t read_len
        );
        extern int64_t etxn_details (
            uint32_t write_ptr,
          	uint32_t write_len
        );
        extern int64_t ledger_seq (void);

        #define PREPARE_PAYMENT_SIMPLE_SIZE 270U
        #define PREPARE_PAYMENT_SIMPLE(buf_out_master, drops_amount_raw, to_address, dest_tag_raw, src_tag_raw)\
            {\
                uint8_t* buf_out = buf_out_master;\
                uint8_t acc[20];\
                uint64_t drops_amount = (drops_amount_raw);\
                uint32_t dest_tag = (dest_tag_raw);\
                uint32_t src_tag = (src_tag_raw);\
                uint32_t cls = (uint32_t)ledger_seq();\
                hook_account(SBUF(acc));\
                _01_02_ENCODE_TT                   (buf_out, ttPAYMENT                      );      /* uint16  | size   3 */ \
                _02_02_ENCODE_FLAGS                (buf_out, tfCANONICAL                    );      /* uint32  | size   5 */ \
                _02_03_ENCODE_TAG_SRC              (buf_out, src_tag                        );      /* uint32  | size   5 */ \
                _02_04_ENCODE_SEQUENCE             (buf_out, 0                              );      /* uint32  | size   5 */ \
                _02_14_ENCODE_TAG_DST              (buf_out, dest_tag                       );      /* uint32  | size   5 */ \
                _02_26_ENCODE_FLS                  (buf_out, cls + 1                        );      /* uint32  | size   6 */ \
                _02_27_ENCODE_LLS                  (buf_out, cls + 5                        );      /* uint32  | size   6 */ \
                _06_01_ENCODE_DROPS_AMOUNT         (buf_out, drops_amount                   );      /* amount  | size   9 */ \
                uint8_t* fee_ptr = buf_out;\
                _06_08_ENCODE_DROPS_FEE            (buf_out, 0                              );      /* amount  | size   9 */ \
                _07_03_ENCODE_SIGNING_PUBKEY_NULL  (buf_out                                 );      /* pk      | size  35 */ \
                _08_01_ENCODE_ACCOUNT_SRC          (buf_out, acc                            );      /* account | size  22 */ \
                _08_03_ENCODE_ACCOUNT_DST          (buf_out, to_address                     );      /* account | size  22 */ \
                int64_t edlen = etxn_details((uint32_t)buf_out, PREPARE_PAYMENT_SIMPLE_SIZE);       /* emitdet | size 1?? */ \
                int64_t fee = etxn_fee_base(buf_out_master, PREPARE_PAYMENT_SIMPLE_SIZE);                                    \
                _06_08_ENCODE_DROPS_FEE            (fee_ptr, fee                            );                               \
            }

        #define UINT16_FROM_BUF(buf)\
            (((uint64_t)((buf)[0]) <<  8U) +\
             ((uint64_t)((buf)[1]) <<  0U))

        #define BUFFER_EQUAL_32(buf1, buf2)\
            (\
                *(((uint64_t*)(buf1)) + 0) == *(((uint64_t*)(buf2)) + 0) &&\
                *(((uint64_t*)(buf1)) + 1) == *(((uint64_t*)(buf2)) + 1) &&\
                *(((uint64_t*)(buf1)) + 2) == *(((uint64_t*)(buf2)) + 2) &&\
                *(((uint64_t*)(buf1)) + 3) == *(((uint64_t*)(buf2)) + 3) &&\
                *(((uint64_t*)(buf1)) + 4) == *(((uint64_t*)(buf2)) + 4) &&\
                *(((uint64_t*)(buf1)) + 5) == *(((uint64_t*)(buf2)) + 5) &&\
                *(((uint64_t*)(buf1)) + 6) == *(((uint64_t*)(buf2)) + 6) &&\
                *(((uint64_t*)(buf1)) + 7) == *(((uint64_t*)(buf2)) + 7))

        #define ASSERT(x)\
             if (!(x))\
                rollback((uint32_t)#x,sizeof(#x),__LINE__)

        #define sfDestination ((8U << 16U) + 3U)

        extern int64_t etxn_generation(void);
        extern int64_t otxn_generation(void);
        extern int64_t otxn_burden(void);
        extern int64_t etxn_burden(void);

        int64_t cbak(uint32_t r)
        {
            // on callback we emit 2 more txns
            uint8_t bob[20];
            ASSERT(otxn_field(SBUF(bob), sfDestination) == 20);

            ASSERT(otxn_generation() + 1 == etxn_generation());

            ASSERT(etxn_burden() == PREREQUISITE_NOT_MET);

            ASSERT(etxn_reserve(2) == 2);
            
            ASSERT(otxn_burden() > 0);
            ASSERT(etxn_burden() == otxn_burden() * 2);

            uint8_t tx[PREPARE_PAYMENT_SIMPLE_SIZE];
            PREPARE_PAYMENT_SIMPLE(tx, 1000, bob, 0, 0);

            uint8_t hash1[32];
            ASSERT(emit(SBUF(hash1), SBUF(tx)) == 32);

            ASSERT(etxn_details(tx + 132, 138) == 138);
            uint8_t hash2[32];
            ASSERT(emit(SBUF(hash2), SBUF(tx)) == 32);

            ASSERT(!BUFFER_EQUAL_32(hash1, hash2)); 

            return accept(0,0,0);
        }

        int64_t hook(uint32_t r)
        {
            _g(1,1);

            etxn_reserve(1);
            
            // bounds checks
            ASSERT(emit(1000000, 32, 0, 32) == OUT_OF_BOUNDS);
            ASSERT(emit(0,1000000, 0, 32) == OUT_OF_BOUNDS);
            ASSERT(emit(0,32, 1000000, 32) == OUT_OF_BOUNDS);
            ASSERT(emit(0,32, 0, 1000000) == OUT_OF_BOUNDS);

            ASSERT(otxn_generation() == 0);
            ASSERT(otxn_burden == 1);

            uint8_t bob[20];
            ASSERT(otxn_param(SBUF(bob), "bob", 3) == 20);

            uint8_t tx[PREPARE_PAYMENT_SIMPLE_SIZE];
            PREPARE_PAYMENT_SIMPLE(tx, 1000, bob, 0, 0);

            uint8_t hash[32];
            ASSERT(emit(SBUF(hash), SBUF(tx)) == 32);

            return accept(0,0,0);
        }
        )[test.hook]"];

        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set emit"),
            HSFEE);
        env.close();
        
        Json::Value invoke;
        invoke[jss::TransactionType] = "Invoke";
        invoke[jss::Account] = alice.human();

        Json::Value params{Json::arrayValue};
        params[0U][jss::HookParameter][jss::HookParameterName ] = strHex(std::string("bob"));
        params[0U][jss::HookParameter][jss::HookParameterValue] = strHex(bob.id());

        invoke[jss::HookParameters] = params;

        env(invoke, M("test emit"), fee(XRP(1)));
        
        std::optional<uint256> emithash;
        {
            auto meta = env.meta(); // meta can close

            // ensure hook execution occured
            BEAST_REQUIRE(meta);
            BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

            auto const hookExecutions =
                meta->getFieldArray(sfHookExecutions);
            BEAST_REQUIRE(hookExecutions.size() == 1);

            // ensure there was one emitted txn
            BEAST_EXPECT(hookExecutions[0].getFieldU16(sfHookEmitCount) == 1);

            BEAST_REQUIRE(meta->isFieldPresent(sfAffectedNodes));

            BEAST_REQUIRE(meta->getFieldArray(sfAffectedNodes).size() == 3);

            for (auto const& node : meta->getFieldArray(sfAffectedNodes))
            {
                SField const& metaType = node.getFName();
                uint16_t nodeType = node.getFieldU16(sfLedgerEntryType); 
                if (metaType == sfCreatedNode && nodeType == ltEMITTED_TXN)
                {
                    BEAST_REQUIRE(node.isFieldPresent(sfNewFields));

                    auto const& nf = 
                        const_cast<ripple::STObject&>(node).getField(sfNewFields).downcast<STObject>();
            
                    auto const& et =
                        const_cast<ripple::STObject&>(nf).getField(sfEmittedTxn).downcast<STObject>();

                    auto const& em =
                        const_cast<ripple::STObject&>(et).getField(sfEmitDetails).downcast<STObject>();
                   
                    BEAST_EXPECT(em.getFieldU32(sfEmitGeneration) == 1);
                    BEAST_EXPECT(em.getFieldU64(sfEmitBurden) == 1);
                    
                    Blob txBlob = et.getSerializer().getData();
                    auto const tx = std::make_unique<STTx>(Slice { txBlob.data(), txBlob.size() });
                    emithash = tx->getTransactionID();
                    
                    break;
                }
            }

            BEAST_REQUIRE(emithash);
        }

        {
            auto balbefore = env.balance(bob).value().xrp().drops();
        
            env.close();

            auto const ledger = env.closed();

            int txcount = 0;
            for (auto& i : ledger->txs)
            {
                auto const& hash = i.first->getTransactionID();
                txcount++;
                BEAST_EXPECT(hash == *emithash);
            }

            BEAST_EXPECT(txcount == 1);

            auto balafter = env.balance(bob).value().xrp().drops();

            BEAST_EXPECT(balafter - balbefore == 1000);

            env.close();
        }
        
        uint64_t burden_expected = 2;
        for (int j = 0; j < 7; ++j)
        {
            auto const ledger = env.closed();
            for (auto& i : ledger->txs)
            {
                auto const& em =
                    const_cast<ripple::STTx&>(*(i.first)).getField(sfEmitDetails).downcast<STObject>();
                BEAST_EXPECT(em.getFieldU64(sfEmitBurden) == burden_expected);
                BEAST_EXPECT(em.getFieldU32(sfEmitGeneration) == j + 2);
                BEAST_REQUIRE(i.second->isFieldPresent(sfHookExecutions));
                auto const hookExecutions = i.second->getFieldArray(sfHookExecutions);
                BEAST_EXPECT(hookExecutions.size() == 1);
                BEAST_EXPECT(hookExecutions[0].getFieldU64(sfHookReturnCode) == 0); 
                BEAST_EXPECT(hookExecutions[0].getFieldU8(sfHookResult) == 3);
                BEAST_EXPECT(hookExecutions[0].getFieldU16(sfHookEmitCount) == 2);
            }
            env.close();
            burden_expected *= 2U;
        }
        
        {    
            auto const ledger = env.closed();
            int txcount = 0;
            for (auto& i : ledger->txs)
            {
                txcount++;
                auto const& em =
                    const_cast<ripple::STTx&>(*(i.first)).getField(sfEmitDetails).downcast<STObject>();
                BEAST_EXPECT(em.getFieldU64(sfEmitBurden) == 256);
                BEAST_EXPECT(em.getFieldU32(sfEmitGeneration) == 9);
                BEAST_REQUIRE(i.second->isFieldPresent(sfHookExecutions));
                auto const hookExecutions = i.second->getFieldArray(sfHookExecutions);
                BEAST_EXPECT(hookExecutions.size() == 1);
                BEAST_EXPECT(hookExecutions[0].getFieldU64(sfHookReturnCode) == 283); // emission failure on first emit
            }
            BEAST_EXPECT(txcount == 256);
        }
        
        // next close will lead to zero transactions
        env.close();
        {    
            auto const ledger = env.closed();
            int txcount = 0;
            for (auto& i : ledger->txs)
                txcount++;
            BEAST_EXPECT(txcount == 0);
        }
    }

    void
    test_etxn_details()
    {
        // mainly tested in test_emit
        testcase("Test etxn_details");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t etxn_details (uint32_t, uint32_t);
            extern int64_t etxn_reserve(uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define PREREQUISITE_NOT_MET -9
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t det[116];

                // Test out of bounds check
                ASSERT(etxn_details(1000000, 116) == OUT_OF_BOUNDS);
                ASSERT(etxn_details(0, 1000000) == OUT_OF_BOUNDS);

                ASSERT(etxn_details((uint32_t)det, 115) == TOO_SMALL);

                ASSERT(etxn_details((uint32_t)det, 116) == PREREQUISITE_NOT_MET);

                etxn_reserve(1);
                ASSERT(etxn_details((uint32_t)det, 116) == 116);

                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set etxn_details"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test etxn_details"), fee(XRP(1)));

    }

    void
    test_etxn_fee_base()
    {
        // mainly tested in test_emit
        testcase("Test etxn_fee_base");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t etxn_fee_base (uint32_t, uint32_t);
            extern int64_t etxn_reserve(uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define PREREQUISITE_NOT_MET -9
            #define INVALID_TXN -37
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reservmaed )
            {
                _g(1,1);
                uint8_t det[116];

                // Test out of bounds check
                ASSERT(etxn_fee_base(1000000, 116) == OUT_OF_BOUNDS);
                ASSERT(etxn_fee_base(0, 1000000) == OUT_OF_BOUNDS);

                ASSERT(etxn_fee_base((uint32_t)det, 116) == PREREQUISITE_NOT_MET);

                etxn_reserve(1);
                ASSERT(etxn_fee_base((uint32_t)det, 116) == INVALID_TXN);

                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set etxn_fee_base"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test etxn_fee_base"), fee(XRP(1)));

    }

    void
    test_etxn_nonce()
    {
        // mainly tested in test_emit
        testcase("Test etxn_nonce");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t etxn_nonce (uint32_t, uint32_t);
            extern int64_t etxn_reserve(uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define TOO_MANY_NONCES -12
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reservmaed )
            {
                _g(1,1);
                uint8_t nonce[64];

                // Test out of bounds check
                ASSERT(etxn_nonce(1000000, 116) == OUT_OF_BOUNDS);
                ASSERT(etxn_nonce(0, 1000000) == OUT_OF_BOUNDS);

                ASSERT(etxn_nonce((uint32_t)nonce, 31) == TOO_SMALL);
            
                uint64_t* n1 = (uint64_t*)nonce;
                uint64_t* n2 = (uint64_t*)(((uint8_t*)nonce) + 32);
        
                for (int i = 0; GUARD(256), i < 256; ++i)
                {
                    ASSERT(etxn_nonce((uint32_t)nonce + ((i % 2) * 32), 32) == 32);
                    ASSERT(!(*(n1 + 0) == *(n2 + 0) && *(n1 + 1) == *(n2 + 1)));
                }

                ASSERT(etxn_nonce((uint32_t)nonce, 116) == TOO_MANY_NONCES);

                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set etxn_nonce"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test etxn_nonce"), fee(XRP(1)));

    }

    void
    test_etxn_reserve()
    {
        // mainly tested in test_emit
        testcase("Test etxn_reserve");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t etxn_reserve(uint32_t);
            #define TOO_BIG -3
            #define TOO_SMALL -4
            #define ALREADY_SET -8
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reservmaed )
            {
                _g(1,1);
                ASSERT(etxn_reserve(0) == TOO_SMALL);
                ASSERT(etxn_reserve(256) == TOO_BIG);
                ASSERT(etxn_reserve(255) == 255);
                ASSERT(etxn_reserve(255) == ALREADY_SET);
                ASSERT(etxn_reserve(1) == ALREADY_SET);
                
                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set etxn_reserve"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test etxn_reserve"), fee(XRP(1)));

    }

    void
    test_fee_base()
    {
        testcase("Test fee_base");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t fee_base(void);
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reservmaed )
            {
                _g(1,1);
                ASSERT(fee_base() == 10); 
                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set fee_base"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test fee_base"), fee(XRP(1)));
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
            TestHook hook = wasm[R"[test.hook](
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

            env(pay(bob, alice, XRP(1)), M("test float_compare"), fee(XRP(1)));
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
            TestHook hook = wasm[R"[test.hook](
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
                #define float_exponent(f) (((int32_t)(((f) >> 54U) & 0xFFU)) - 97)
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
                    if (diffexp > 1 || diffman > 5000000 || mx < 0 || my < 0)\
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

            env(pay(bob, alice, XRP(1)), M("test float_divide"), fee(XRP(1)));
            env.close();
        }
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
            TestHook hook = wasm[R"[test.hook](
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

            env(pay(bob, alice, XRP(1)), M("test float_int"), fee(XRP(1)));
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
            TestHook hook = wasm[R"[test.hook](
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
                #define float_exponent(f) (((int32_t)(((f) >> 54U) & 0xFFU)) - 97)
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
                    if (diffexp > 1 || diffman > 5000000 || mx < 0 || my < 0)\
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

            env(pay(bob, alice, XRP(1)), M("test float_invert"), fee(XRP(1)));
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
            TestHook hook = wasm[R"[test.hook](
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
                #define float_exponent(f) (((int32_t)(((f) >> 54U) & 0xFFU)) - 97)
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
                    if (diffexp > 1 || diffman > 5000000 || mx < 0 || my < 0)\
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

            env(pay(bob, alice, XRP(1)), M("test float_log"), fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_mantissa()
    {
        testcase("Test float_mantissa");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_one (void);
                extern int64_t float_mantissa(int64_t);
                extern int64_t float_negate(int64_t);
                #define ASSERT_EQUAL(x,y)\
                    if ((x) != (y))\
                        rollback(0,0,__LINE__);
                #define ASSERT(x)\
                    if (!(x))\
                        rollback(0,0,__LINE__);
                #define INVALID_FLOAT -10024
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    int64_t result = 0;

                    // test invalid floats
                    {
                        ASSERT(float_mantissa(-1) == INVALID_FLOAT);
                        ASSERT(float_mantissa(-11010191919LL) == INVALID_FLOAT);
                    }

                    // test canonical zero
                    ASSERT(float_mantissa(0) == 0);

                    // test one, negative one
                    {
                        ASSERT(float_mantissa(float_one()) == 1000000000000000LL);
                        ASSERT(float_mantissa(float_negate(float_one())) == 1000000000000000LL);
                    }

                    // test random numbers
                    {
                        ASSERT_EQUAL(
                            float_mantissa(4763370308433150973LL /* 7.569101929907197e-74 */),
                            7569101929907197LL);
                        ASSERT_EQUAL(
                            float_mantissa(668909658849475214LL /* -2.376913998641806e-45 */),
                            2376913998641806LL);
                        ASSERT_EQUAL(
                            float_mantissa(962271544155031248LL /* -7.508423152486096e-29 */),
                            7508423152486096LL);
                        ASSERT_EQUAL(
                            float_mantissa(7335644976228470276LL /* 3.784782869302788e+69 */),
                            3784782869302788LL);
                        ASSERT_EQUAL(
                            float_mantissa(2837780149340315954LL /* -9.519583351644467e+75 */),
                            9519583351644466LL);
                        ASSERT_EQUAL(
                            float_mantissa(2614004940018599738LL /* -1.917156143712058e+63 */),
                            1917156143712058LL);
                        ASSERT_EQUAL(
                            float_mantissa(4812250541755005603LL /* 2.406139723315875e-71 */),
                            2406139723315875LL);
                        ASSERT_EQUAL(
                            float_mantissa(5140304866732560580LL /* 6.20129153019514e-53 */),
                            6201291530195140LL);
                        ASSERT_EQUAL(
                            float_mantissa(1124677839589482624LL /* -7.785132001599617e-20 */),
                            7785132001599616LL);
                        ASSERT_EQUAL(
                            float_mantissa(5269336076015865585LL /* 9.131711247126257e-46 */),
                            9131711247126257LL);
                        ASSERT_EQUAL(
                            float_mantissa(2296179634826760368LL /* -8.3510241225484e+45 */),
                            8351024122548400LL);
                        ASSERT_EQUAL(
                            float_mantissa(1104028240398536470LL /* -5.149931320135446e-21 */),
                            5149931320135446LL);
                        ASSERT_EQUAL(
                            float_mantissa(2691222059222981864LL /* -7.076681310166248e+67 */),
                            7076681310166248LL);
                        ASSERT_EQUAL(
                            float_mantissa(6113256168823855946LL /* 63.7507410946337 */),
                            6375074109463370LL);
                        ASSERT_EQUAL(
                            float_mantissa(311682216630003626LL /* -5.437441968809898e-65 */),
                            5437441968809898LL);
                        ASSERT_EQUAL(
                            float_mantissa(794955605753965262LL /* -2.322071336757966e-38 */),
                            2322071336757966LL);
                        ASSERT_EQUAL(
                            float_mantissa(204540636400815950LL /* -6.382252796514126e-71 */),
                            6382252796514126LL);
                        ASSERT_EQUAL(
                            float_mantissa(5497195278343034975LL /* 2.803732951029855e-33 */),
                            2803732951029855LL);
                        ASSERT_EQUAL(
                            float_mantissa(1450265914369875626LL /* -0.09114033611316906 */),
                            9114033611316906LL);
                        ASSERT_EQUAL(
                            float_mantissa(7481064015089962668LL /* 5.088633654939308e+77 */),
                            5088633654939308LL);
                    }

                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_mantissa"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)), M("test float_mantissa"), fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_mulratio()
    {
        testcase("Test float_mulratio");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_mulratio (int64_t, uint32_t, uint32_t, uint32_t);
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
                #define float_exponent(f) (((int32_t)(((f) >> 54U) & 0xFFU)) - 97)
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
                    if (diffexp > 1 || diffman > 5000000 || mx < 0 || my < 0)\
                        rollback((uint32_t) #x, sizeof(#x), __LINE__);\
                }
                int64_t hook(uint32_t reserved )
                {
                   _g(1,1);

                    // ensure invalid xfl are not accepted
                    ASSERT(float_mulratio(-1, 0, 1, 1) == INVALID_FLOAT);

                    // multiply by 0
                    ASSERT(float_mulratio(float_one(), 0, 0, 1) == 0);
                    ASSERT(float_mulratio(0, 0, 1, 1) == 0);

                    // check 1
                    ASSERT(float_mulratio(float_one(), 0, 1, 1) == float_one());
                    ASSERT(float_mulratio(float_negate(float_one()), 0, 1, 1) ==
                            float_negate(float_one()));

                    // check overflow
                        // 1e+95 * 1e+95
                    ASSERT(float_mulratio(7801234554605699072LL, 0, 0xFFFFFFFFUL, 1) == XFL_OVERFLOW);
                        // 1e+95 * 10
                    ASSERT(float_mulratio(7801234554605699072LL, 0, 10, 1) == XFL_OVERFLOW);
                        // -1e+95 * 10
                    ASSERT(float_mulratio(3189548536178311168LL, 0, 10, 1) == XFL_OVERFLOW);

                    // identity
                    ASSERT_EQUAL(float_mulratio(3189548536178311168LL, 0, 1, 1), 3189548536178311168LL);


                    // random mulratios
                    ASSERT_EQUAL(
                        float_mulratio(2296131684119423544LL, 0U, 2210828011U, 2814367554U),
                        2294351094683836182LL);
                    ASSERT_EQUAL(
                        float_mulratio(565488225163275031LL, 0U, 2373474507U, 4203973264U),
                        562422045628095449LL);
                    ASSERT_EQUAL(
                        float_mulratio(2292703263479286183LL, 0U, 3170020147U, 773892643U),
                        2307839765178024100LL);
                    ASSERT_EQUAL(
                        float_mulratio(758435948837102675LL, 0U, 3802740780U, 1954123588U),
                        760168290112163547LL);
                    ASSERT_EQUAL(
                        float_mulratio(3063742137774439410LL, 0U, 2888815591U, 4122448592U),
                        3053503824756415637LL);
                    ASSERT_EQUAL(
                        float_mulratio(974014561126802184LL, 0U, 689168634U, 3222648522U),
                        957408554638995792LL);
                    ASSERT_EQUAL(
                        float_mulratio(2978333847445611553LL, 0U, 1718558513U, 2767410870U),
                        2976075722223325259LL);
                    ASSERT_EQUAL(
                        float_mulratio(6577058837932757648LL, 0U, 1423256719U, 1338068927U),
                        6577173649752398013LL);
                    ASSERT_EQUAL(
                        float_mulratio(2668681541248816636LL, 0U, 345215754U, 4259223936U),
                        2650183845127530219LL);
                    ASSERT_EQUAL(
                        float_mulratio(651803640367065917LL, 0U, 327563234U, 1191613855U),
                        639534906402789368LL);
                    ASSERT_EQUAL(
                        float_mulratio(3154958130393015979LL, 0U, 1304112625U, 3024066701U),
                        3153571282364880740LL);
                    ASSERT_EQUAL(
                        float_mulratio(1713286099776800976LL, 0U, 1902151138U, 2927030061U),
                        1712614441093927706LL);
                    ASSERT_EQUAL(
                        float_mulratio(2333142120591277120LL, 0U, 914099656U, 108514965U),
                        2349692988167140475LL);
                    ASSERT_EQUAL(
                        float_mulratio(995968561418010814LL, 0U, 1334462574U, 846156977U),
                        998955931389416094LL);
                    ASSERT_EQUAL(
                        float_mulratio(6276035843030312442LL, 0U, 2660687613U, 236740983U),
                        6294920527635363073LL);
                    ASSERT_EQUAL(
                        float_mulratio(7333118474702086419LL, 0U, 46947714U, 2479204760U),
                        7298214153648998535LL);
                    ASSERT_EQUAL(
                        float_mulratio(2873297486994296492LL, 0U, 880591893U, 436034100U),
                        2884122995598532757LL);
                    ASSERT_EQUAL(
                        float_mulratio(1935815261812737573LL, 0U, 3123665800U, 3786746543U),
                        1934366328810191207LL);
                    ASSERT_EQUAL(
                        float_mulratio(7249556282125616118LL, 0U, 2378803159U, 2248850590U),
                        7250005170160875417LL);
                    ASSERT_EQUAL(
                        float_mulratio(311005347529659996LL, 0U, 992915590U, 2433548552U),
                        308187142737041830LL);

                    // today: round up test
                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_mulratio"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)), M("test float_mulratio"), fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_multiply()
    {
        testcase("Test float_multiply");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_multiply (int64_t, int64_t);
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
                #define float_exponent(f) (((int32_t)(((f) >> 54U) & 0xFFU)) - 97)
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
                    if (diffexp > 1 || diffman > 5000000 || mx < 0 || my < 0)\
                        rollback((uint32_t) #x, sizeof(#x), __LINE__);\
                }
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // ensure invalid xfl are not accepted
                    ASSERT(float_multiply(-1, float_one()) == INVALID_FLOAT);

                    // multiply by 0
                    ASSERT(float_multiply(float_one(), 0) == 0);
                    ASSERT(float_multiply(0, float_one()) == 0);

                    // check 1
                    ASSERT(float_multiply(float_one(), float_one()) == float_one());
                    ASSERT(float_multiply(float_one(), float_negate(float_one())) == float_negate(float_one()));
                    ASSERT(float_multiply(float_negate(float_one()), float_one()) == float_negate(float_one()));
                    ASSERT(float_multiply(float_negate(float_one()), float_negate(float_one())) == float_one());

                    // check overflow
                        // 1e+95 * 1e+95
                    ASSERT(float_multiply(7801234554605699072LL, 7801234554605699072LL) == XFL_OVERFLOW);
                        // 1e+95 * 10
                    ASSERT(float_multiply(7801234554605699072LL, 6107881094714392576LL) == XFL_OVERFLOW);
                    ASSERT(float_multiply(6107881094714392576LL, 7801234554605699072LL) == XFL_OVERFLOW);
                        // -1e+95 * 10
                    ASSERT(float_multiply(3189548536178311168LL, 6107881094714392576LL) == XFL_OVERFLOW);

                    // identity
                    ASSERT_EQUAL(float_multiply(3189548536178311168LL, float_one()), 3189548536178311168LL);
                    ASSERT_EQUAL(float_multiply(float_one(), 3189548536178311168LL), 3189548536178311168LL);


                    // random multiplications
                    ASSERT_EQUAL(
                        float_multiply(
                            7791757438262485039LL /* 9.537282166267951e+94 */,
                            4759088999670263908LL /* 3.287793167020132e-74 */),
                        6470304726017852129LL /* 3.135661113819873e+21 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            7534790022873909775LL /* 4.771445910440463e+80 */,
                            1017891960669847079LL /* -9.085644138855975e-26 */),
                        2472307761756037979LL /* -4.335165957006171e+55 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2813999069907898454LL /* -3.75290242870895e+74 */,
                            4962524721184225460LL /* 8.56513107667986e-63 */),
                        1696567870013294731LL /* -3214410121988.235 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2151742066453140308LL /* -8.028643824784212e+37 */,
                            437647738130579252LL /* -5.302173903011636e-58 */),
                        5732835652591705549LL /* 4.256926576434637e-20 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            5445302332922546340LL /* 4.953983058987172e-36 */,
                            7770966530708354172LL /* 6.760773121619068e+93 */),
                        7137051085305881332LL /* 3.349275551015668e+58 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2542989542826132533LL /* -2.959352989172789e+59 */,
                            6308418769944702613LL /* 3379291626008.213 */),
                        2775217422137696934LL /* -1.000051677471398e+72 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            5017652318929433511LL /* 9.649533293441959e-60 */,
                            6601401767766764916LL /* 8.131913296358772e+28 */),
                        5538267259220228820LL /* 7.846916809259732e-31 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            892430323307269235LL /* -9.724796342652019e-33 */,
                            1444078017997143500LL /* -0.0292613723858478 */),
                        5479222755754111850LL /* 2.845608871588714e-34 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            7030632722283214253LL /* 5.017303585240493e+52 */,
                            297400838197636668LL /* -9.170462045924924e-66 */),
                        1247594596364389994LL /* -4.601099210133098e-13 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            1321751204165279730LL /* -6.700112973094898e-9 */,
                            2451801790748530375LL /* -1.843593458980551e+54 */),
                        6918764256086244704LL /* 1.235228445162848e+46 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2055496484261758590LL /* -1.855054180812414e+32 */,
                            2079877890137711361LL /* -8.222061547283201e+33 */),
                        7279342234795540005LL /* 1.525236964818469e+66 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2439875962311968674LL /* -7.932163531900834e+53 */,
                            4707485682591872793LL /* 5.727671617074969e-77 */),
                        1067392794851803610LL /* -4.543282792366554e-23 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            6348574818322812800LL /* 750654298515443.2 */,
                            6474046245013515838LL /* 6.877180109483582e+21 */),
                        6742547427357110773LL /* 5.162384810848757e+36 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            1156137305783593424LL /* -3.215801176746448e-18 */,
                            351790564990861307LL /* -9.516993310703611e-63 */),
                        4650775291275116747LL /* 3.060475828764875e-80 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            5786888485280994123LL /* 4.266563737277259e-17 */,
                            6252137323085080394LL /* 1141040294.831946 */),
                        5949619829273756852LL /* 4.868321144702132e-8 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2078182880999439640LL /* -6.52705240901148e+33 */,
                            1662438186251269392LL /* -51135233789.26864 */),
                        6884837854131013998LL /* 3.33762350889611e+44 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            1823781083140711248LL /* -43268336830308640000 */,
                            1120252241608199010LL /* -3.359534020316002e-20 */),
                        6090320310700749729LL /* 1.453614495839137 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            6617782604883935174LL /* 6.498351904047046e+29 */,
                            6185835042802056262LL /* 689635.404973575 */),
                        6723852137583788319LL /* 4.481493547008287e+35 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            333952667495151166LL /* -9.693494324475454e-64 */,
                            1556040883317758614LL /* -68026.1150230799 */),
                        5032611291744396930LL /* 6.594107598923394e-59 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2326968399632616779LL /* -3.110991909440843e+47 */,
                            707513695207834635LL /* -4.952153338037259e-43 */),
                        6180479299649214949LL /* 154061.0896894437 */);


                    ASSERT_EQUAL(
                        float_multiply(
                            1271003508324696477LL /* -9.995612660957597e-12 */,
                            5321949753651889765LL /* 7.702193354704484e-43 */),
                        512101972406838314LL /* -7.698814141342762e-54 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            1928646740923345323LL /* -1.106100408773035e+25 */,
                            4639329980209973352LL /* 9.629563273103463e-81 */),
                        487453886143282122LL /* -1.065126387268554e-55 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            6023906813956669432LL /* 0.0007097711789686777 */,
                            944348444470060009LL /* -7.599721976996842e-30 */),
                        888099590592064434LL /* -5.394063627447218e-33 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            6580290597764062787LL /* 5.035141803138627e+27 */,
                            6164319297265300034LL /* 33950.07022461506 */),
                        6667036882686408593LL /* 1.709434178074513e+32 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2523439530503240484LL /* -1.423739175762724e+58 */,
                            5864448766677980801LL /* 9.769251096336e-13 */),
                        2307233895764065602LL /* -1.39088655037165e+46 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            6760707453987140465LL /* 5.308012931396465e+37 */,
                            5951641080643457645LL /* 6.889572514402925e-8 */),
                        6632955645489194550LL /* 3.656993999824438e+30 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            6494270716308443375LL /* 9.087252894929135e+22 */,
                            564752637895553836LL /* -6.306284101612332e-51 */),
                        978508199357889360LL /* -5.730679845862224e-28 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            6759145618427534062LL /* 3.746177371790062e+37 */,
                            4721897842483633304LL /* 2.125432999353496e-76 */),
                        5394267403342547165LL /* 7.962249007433949e-39 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            1232673571201806425LL /* -7.694472557031513e-14 */,
                            6884256144221925318LL /* 2.75591359980743e+44 */),
                        2037747561727791012LL /* -2.12053015632682e+31 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            1427694775835421031LL /* -0.004557293586344295 */,
                            4883952867277976402LL /* 2.050871208358738e-67 */),
                        225519204318055258LL /* -9.34642220427145e-70 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            5843509949864662087LL /* 6.84483279249927e-14 */,
                            5264483986612843822LL /* 4.279621844104494e-46 */),
                        5028946513739275800LL /* 2.929329593802264e-59 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            6038444022009738988LL /* 0.003620521333274348 */,
                            7447499078040748850LL /* 7.552493624689458e+75 */),
                        7406652183825856093LL /* 2.734396428760669e+73 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            939565473697468970LL /* -2.816751204405802e-30 */,
                            1100284903077087966LL /* -1.406593998686942e-21 */),
                        5174094397561240825LL /* 3.962025339911417e-51 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            5694071830210473617LL /* 1.521901214166673e-22 */,
                            5536709154363579683LL /* 6.288811952610595e-31 */),
                        5143674525748709391LL /* 9.570950546343951e-53 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            600729862341871819LL /* -6.254711528966347e-49 */,
                            6330630279715378440LL /* 75764028872020.56 */),
                        851415551394320910LL /* -4.738821448667662e-35 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            1876763139233864902LL /* -3.265694247738566e+22 */,
                            4849561230315278754LL /* 3.688031264625058e-69 */),
                        649722744589988028LL /* -1.204398248636604e-46 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            3011947542126279863LL /* -3.542991042788535e+85 */,
                            1557732559110376235LL /* -84942.87294925611 */),
                        7713172080438368541LL /* 3.009518380079389e+90 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            5391579936313268788LL /* 5.274781978155572e-39 */,
                            1018647290024655822LL /* -9.840973493664718e-26 */),
                        329450072133864644LL /* -5.190898963188932e-64 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            2815029221608845312LL /* -4.783054129655808e+74 */,
                            4943518985822088837LL /* 7.57379422402522e-64 */),
                        1678961648155863225LL /* -362258677403.8713 */);
                    ASSERT_EQUAL(
                        float_multiply(
                            1377509900308195934LL /* -0.00000841561358756515 */,
                            7702104197062186199LL /* 9.95603351337903e+89 */),
                        2998768765665354000LL /* -8.378613091344656e+84 */);

                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_multiply"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)), M("test float_multiply"), fee(XRP(1)));
            env.close();
        }
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
            TestHook hook = wasm[R"[test.hook](
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

            env(pay(bob, alice, XRP(1)), M("test float_negate"), fee(XRP(1)));
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
            TestHook hook = wasm[R"[test.hook](
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

            env(pay(bob, alice, XRP(1)), M("test float_one"), fee(XRP(1)));
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
            TestHook hook = wasm[R"[test.hook](
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
                #define float_exponent(f) (((int32_t)(((f) >> 54U) & 0xFFU)) - 97)
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
                    if (diffexp > 1 || diffman > 5000000 || mx < 0 || my < 0)\
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

            env(pay(bob, alice, XRP(1)), M("test float_root"), fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_set()
    {
        testcase("Test float_set");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_set (int32_t, int64_t);
                #define INVALID_FLOAT -10024
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);
                    // zero mantissa should return canonical zero
                    ASSERT(float_set(-5, 0) == 0);
                    ASSERT(float_set(50, 0) == 0);
                    ASSERT(float_set(-50, 0) == 0);
                    ASSERT(float_set(0, 0) == 0);

                    // an exponent lower than -96 should produce an invalid float error
                    ASSERT(float_set(-97, 1) == INVALID_FLOAT);

                    // an exponent larger than +96 should produce an invalid float error
                    ASSERT(float_set(+97, 1) == INVALID_FLOAT);

                    ASSERT(float_set(-5,6541432897943971LL) == 6275552114197674403LL);
                    ASSERT(float_set(-83,7906202688397446LL) == 4871793800248533126LL);
                    ASSERT(float_set(76,4760131426754533LL) == 7732937091994525669LL);
                    ASSERT(float_set(37,-8019384286534438LL) == 2421948784557120294LL);
                    ASSERT(float_set(50,5145342538007840LL) == 7264947941859247392LL);
                    ASSERT(float_set(-70,4387341302202416LL) == 5102462119485603888LL);
                    ASSERT(float_set(-26,-1754544005819476LL) == 1280776838179040340LL);
                    ASSERT(float_set(36,8261761545780560LL) == 7015862781734272336LL);
                    ASSERT(float_set(35,7975622850695472LL) == 6997562244529705264LL);
                    ASSERT(float_set(17,-4478222822793996LL) == 2058119652903740172LL);
                    ASSERT(float_set(-53,5506604247857835LL) == 5409826157092453035LL);
                    ASSERT(float_set(-60,5120164869507050LL) == 5283338928147728362LL);
                    ASSERT(float_set(41,5176113875683063LL) == 7102849126611584759LL);
                    ASSERT(float_set(-54,-3477931844992923LL) == 778097067752718235LL);
                    ASSERT(float_set(21,6345031894305479LL) == 6743730074440567495LL);
                    ASSERT(float_set(-23,5091583691147091LL) == 5949843091820201811LL);
                    ASSERT(float_set(-33,7509684078851678LL) == 5772117207113086558LL);
                    ASSERT(float_set(-72,-1847771838890268LL) == 452207734575939868LL);
                    ASSERT(float_set(71,-9138413713437220LL) == 3035557363306410532LL);
                    ASSERT(float_set(28,4933894067102586LL) == 6868419726179738490LL);

                    return accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_set"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)), M("test float_set"), fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_sign()
    {
        testcase("Test float_sign");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t float_one (void);
                extern int64_t float_sign(int64_t);
                extern int64_t float_negate(int64_t);
                #define ASSERT(x)\
                    if ((x) != 1)\
                        rollback(0,0,__LINE__)
                #define ASSERT_EQUAL(x,y) ASSERT((x) == (y))
                #define INVALID_FLOAT -10024
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    int64_t result = 0;

                    // test invalid floats
                    {
                        ASSERT(float_sign(-1) == INVALID_FLOAT);
                        ASSERT(float_sign(-11010191919LL) == INVALID_FLOAT);
                    }

                    // test canonical zero
                    ASSERT(float_sign(0) == 0);

                    // test one
                    ASSERT(float_sign(float_one()) == 0);
                    ASSERT(float_sign(float_negate(float_one())) == 1);

                    // test random numbers
                    ASSERT_EQUAL(
                        float_sign(7248434512952957686LL /* 6.646312141200119e+64 */),
                        0LL);
                    ASSERT_EQUAL(
                        float_sign(889927818394811978LL /* -7.222291430194763e-33 */),
                        1LL);
                    ASSERT_EQUAL(float_sign(5945816149233111421LL /* 1.064641104056701e-8 */), 0LL);
                    ASSERT_EQUAL(float_sign(6239200145838704863LL /* 621826155.7938399 */), 0LL);
                    ASSERT_EQUAL(
                        float_sign(6992780785042190360LL /* 3.194163363180568e+50 */),
                        0LL);
                    ASSERT_EQUAL(
                        float_sign(6883099933108789087LL /* 1.599702486671199e+44 */),
                        0LL);
                    ASSERT_EQUAL(
                        float_sign(890203738162163464LL /* -7.498211197546248e-33 */),
                        1LL);
                    ASSERT_EQUAL(float_sign(4884803073052080964LL /* 2.9010769824633e-67 */), 0LL);
                    ASSERT_EQUAL(
                        float_sign(2688292350356944394LL /* -4.146972444128778e+67 */),
                        1LL);
                    ASSERT_EQUAL(
                        float_sign(4830109852288093280LL /* 2.251051746921568e-70 */),
                        0LL);
                    ASSERT_EQUAL(
                        float_sign(294175951907940320LL /* -5.945575756228576e-66 */),
                        1LL);
                    ASSERT_EQUAL(
                        float_sign(7612037404955382316LL /* 9.961233953985069e+84 */),
                        0LL);
                    ASSERT_EQUAL(float_sign(7520840929603658997LL /* 8.83675114967167e+79 */), 0LL);
                    ASSERT_EQUAL(
                        float_sign(4798982086157926282LL /* 7.152082635718538e-72 */),
                        0LL);
                    ASSERT_EQUAL(
                        float_sign(689790136568817905LL /* -5.242993208502513e-44 */),
                        1LL);
                    ASSERT_EQUAL(
                        float_sign(5521738045011558042LL /* 9.332101110070938e-32 */),
                        0LL);
                    ASSERT_EQUAL(
                        float_sign(728760820583452906LL /* -8.184880204173546e-42 */),
                        1LL);
                    ASSERT_EQUAL(
                        float_sign(2272937984362856794LL /* -3.12377216812681e+44 */),
                        1LL);
                    ASSERT_EQUAL(float_sign(1445723661896317830LL /* -0.0457178113775911 */), 1LL);
                    ASSERT_EQUAL(
                        float_sign(5035721527359772724LL /* 9.704343214299189e-59 */),
                        0LL);

                    return
                        accept(0,0,0);
                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_sign"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)), M("test float_sign"), fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_sto()
    {
        testcase("Test float_sto");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t hook_account (uint32_t, uint32_t);
                extern int64_t float_sto (
                    uint32_t write_ptr,
                    uint32_t write_len,
                    uint32_t cread_ptr,
                    uint32_t cread_len,  
                    uint32_t iread_ptr,
                    uint32_t iread_len,  
                    int64_t float1,
                    uint32_t field_code
                );
                extern int64_t float_sto_set (
                    uint32_t read_ptr,
                    uint32_t read_len
                );
                #define OUT_OF_BOUNDS (-1)
                #define INVALID_FLOAT (-10024)
                #define INVALID_ARGUMENT (-7)
                #define TOO_SMALL (-4)
                #define XFL_OVERFLOW (-30)
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);

                #define SBUF(x) x, sizeof(x)
                #define sfAmount ((6U << 16U) + 1U)
                #define sfDeliveredAmount ((6U << 16U) + 18U)
                uint8_t cur1[3] = {'U','S','D'};
                uint8_t cur1full[20] = {
                    0,0,0,0,0,0,0,0,0,0,0,0,
                    'U', 'S', 'D',
                    0,0,0,0,0
                };

                #define BUFFER_EQUAL_20(buf1, buf2)\
                    (\
                        *(((uint64_t*)(buf1)) + 0)) == *(((uint64_t*)(buf2)) + 0) &&\
                        *(((uint64_t*)(buf1)) + 1) == *(((uint64_t*)(buf2)) + 1) &&\
                        *(((uint32_t*)(buf1)) + 4) == *(((uint32_t*)(buf2)) + 4)           
 
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);
                    int64_t y;
                    uint8_t cur2[20];
                    for (int i =0; GUARD(20), i < 20; ++i)
                        cur2[i] = i;

                    uint8_t iss[20];
                    ASSERT(hook_account(SBUF(iss)) == 20);


                    uint8_t buf[50];

                    // the three buffers must be bounds checked
                    ASSERT((y=float_sto(1000000, 50, 0,0,0,0,0,0)) == OUT_OF_BOUNDS);
                    ASSERT((y=float_sto(0, 1000000, 0,0,0,0,0,0)) == OUT_OF_BOUNDS);
                    ASSERT((y=float_sto(SBUF(buf), 1000000, 20, 1,20,0,0)) == OUT_OF_BOUNDS);
                    ASSERT((y=float_sto(SBUF(buf), 1, 1000000, 1,20,0,0)) == INVALID_ARGUMENT);
                    ASSERT((y=float_sto(SBUF(buf), 1,20, 1000000, 20, 0,0)) == OUT_OF_BOUNDS);
                    ASSERT((y=float_sto(SBUF(buf), 1,20, 1, 1000000, 0,0)) == INVALID_ARGUMENT);

                    // zero issuer/currency pointers must be accompanied by 0 length
                    ASSERT((y=float_sto(SBUF(buf), 0, 1, 0,0, 0,0)) == INVALID_ARGUMENT);
                    ASSERT((y=float_sto(SBUF(buf), 0, 0, 0,1, 0,0)) == INVALID_ARGUMENT);
                    
                    // zero issuer/currency lengths mus tbe accompanied by 0 pointers
                    ASSERT((y=float_sto(SBUF(buf), 1, 0, 0,0, 0,0)) == INVALID_ARGUMENT);
                    ASSERT((y=float_sto(SBUF(buf), 0, 0, 1,0, 0,0)) == INVALID_ARGUMENT);

                    // issuer without currency is invalid
                    ASSERT((y=float_sto(SBUF(buf), 0,0, SBUF(iss), 0, sfAmount)) == INVALID_ARGUMENT);
                    
                    // currency without issuer is invalid
                    ASSERT((y=float_sto(SBUF(buf), SBUF(cur1), 0,0, 0, sfAmount)) == INVALID_ARGUMENT);

                    // currency and issuer with field code 0 = XRP is invalid
                    ASSERT((y=float_sto(SBUF(buf), SBUF(cur1), SBUF(iss), 0, 0)) == INVALID_ARGUMENT);
       
                    // invalid XFL
                    ASSERT((y=float_sto(SBUF(buf), SBUF(cur2), SBUF(iss), -1, sfAmount)) == INVALID_FLOAT);

                    // valid XFL, currency and issuer
                    { 
                        // currency and issuer with field code not XRP is valid (XFL = 1234567.0)
                        // try with a three letter currency
                        ASSERT((y=float_sto(SBUF(buf), SBUF(cur1), SBUF(iss), 6198187654261802496ULL, sfAmount)) == 49);
                        
                        // check the output contains the correct currency code
                        ASSERT(BUFFER_EQUAL_20(buf + 9, cur1full));

                        // again with a 20 byte currency
                        ASSERT((y=float_sto(SBUF(buf), SBUF(cur2), SBUF(iss), 6198187654261802496ULL, sfAmount)) == 49);
                    
                        // check the output contains the correct currency code
                        ASSERT(BUFFER_EQUAL_20(buf + 9, cur2));
                        
                        // check the output contains the correct issuer
                        ASSERT(BUFFER_EQUAL_20(buf + 29, iss));

                        // check the field code is correct
                        ASSERT(buf[0] == 0x61U); // sfAmount

                        // reverse the operation and check the XFL amount is correct
                        ASSERT((y=float_sto_set(buf, 49)) == 6198187654261802496ULL);

                        // test 0
                        ASSERT((y=float_sto(SBUF(buf), SBUF(cur2), SBUF(iss), 0, sfAmount)) == 49);
                        ASSERT((y=float_sto_set(buf, 49)) == 0);

                    }
                        
                    // the same again but with a field-uncommon fieldcode
                    {
                        ASSERT((y=float_sto(SBUF(buf), SBUF(cur2), SBUF(iss), 6198187654261802496ULL, sfDeliveredAmount)) == 50);
                    
                        // check the first 2 bytes
                        ASSERT(buf[0] == 0x60U && buf[1] == 0x12U);

                        // same checks as above moved along one
                        
                        // check the output contains the correct currency code
                        ASSERT(BUFFER_EQUAL_20(buf + 10, cur2));
                        
                        // check the output contains the correct issuer
                        ASSERT(BUFFER_EQUAL_20(buf + 30, iss));
                        
                        // reverse the operation and check the XFL amount is correct
                        ASSERT((y=float_sto_set(SBUF(buf))) == 6198187654261802496ULL);
                    }

                    // and the same again except use -1 as field code to supress field type bytes
                    {
                        // zero the serialized amount bytes
                        *((uint64_t*)(buf + 2)) = 0;

                        // request fieldcode -1 = only serialize the number
                        ASSERT((y=float_sto(buf + 2, 8, 0,0,0,0, 6198187654261802496ULL, 0xFFFFFFFFUL)) == 8);
                    
                        // reverse the operation and check the XFL amount is correct
                        ASSERT((y=float_sto_set(SBUF(buf))) == 6198187654261802496ULL);

                        // try again with some different xfls
                        ASSERT((y=float_sto(buf + 2, 8, 0,0,0,0, 1244912689067196128ULL, 0xFFFFFFFFUL)) == 8);
                        ASSERT((y=float_sto_set(SBUF(buf))) == 1244912689067196128ULL);


                        // test 0
                        ASSERT((y=float_sto(buf + 2, 8, 0,0,0,0, 0, 0xFFFFFFFFUL)) == 8);
                        ASSERT((y=float_sto_set(SBUF(buf))) == 0);
                    }

                    // finally test xrp
                    {
                        // zero the serialized amount bytes
                        *((uint64_t*)(buf)) = 0;

                        // request fieldcode 0 = xrp amount serialized
                        ASSERT((y=float_sto(buf + 1, 8, 0,0,0,0, 6198187654261802496ULL, 0)) == 8);

                        buf[0] = 0x61U;
                
                        ASSERT((y=float_sto_set(buf, 9)) == 6198187654261802496ULL);

                        // test 0
                        ASSERT((y=float_sto(buf + 1, 8, 0,0,0,0, 0, 0)) == 8);
                        ASSERT((y=float_sto_set(buf, 9)) == 0);
                        
                        //6198187654373024496
                    }
               
                    return accept(0,0,0); 

                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_sto"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)), M("test float_sto"), fee(XRP(1)));
            env.close();
        }
    }

    void
    test_float_sto_set()
    {
        testcase("Test float_sto_set");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t hook_account (uint32_t, uint32_t);
                extern int64_t float_sto_set (
                    uint32_t read_ptr,
                    uint32_t read_len
                );
                #define NOT_AN_OBJECT (-23)
                #define OUT_OF_BOUNDS (-1)
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);

                #define SBUF(x) x, sizeof(x)

                #define BUFFER_EQUAL_20(buf1, buf2)\
                    (\
                        *(((uint64_t*)(buf1)) + 0)) == *(((uint64_t*)(buf2)) + 0) &&\
                        *(((uint64_t*)(buf1)) + 1) == *(((uint64_t*)(buf2)) + 1) &&\
                        *(((uint32_t*)(buf1)) + 4) == *(((uint32_t*)(buf2)) + 4)           


                // 1234567000000000 * 10**-9, currency USD, issuer 7C4C8D5B2FDA1D16E9A4F5BB579AC2926C146235 (alice)
                uint8_t iou[] = //6198187654261802496 
                {
                    0x61U,0xD6U,0x04U,0x62U,0xD5U,0x07U,0x7CU,0x86U,0x00U,0x00U,
                    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
                    0x00U,0x55U,0x53U,0x44U,0x00U,0x00U,0x00U,0x00U,0x00U,0x7CU,
                    0x4CU,0x8DU,0x5BU,0x2FU,0xDAU,0x1DU,0x16U,0xE9U,0xA4U,0xF5U,
                    0xBBU,0x57U,0x9AU,0xC2U,0x92U,0x6CU,0x14U,0x62U,0x35U
                };

                // as above but value is negative
                uint8_t iou_neg[] = //1586501635834414592
                {
                    0x61U,0x96U,0x04U,0x62U,0xD5U,0x07U,0x7CU,0x86U,0x00U,0x00U,
                    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
                    0x00U,0x55U,0x53U,0x44U,0x00U,0x00U,0x00U,0x00U,0x00U,0x7CU,
                    0x4CU,0x8DU,0x5BU,0x2FU,0xDAU,0x1DU,0x16U,0xE9U,0xA4U,0xF5U,
                    0xBBU,0x57U,0x9AU,0xC2U,0x92U,0x6CU,0x14U,0x62U,0x35U
                };
        
                // as above but value is 0
                uint8_t iou_zero[] = // 0
                {
                    0x61U,0x80U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
                    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
                    0x00U,0x55U,0x53U,0x44U,0x00U,0x00U,0x00U,0x00U,0x00U,0x7CU,
                    0x4CU,0x8DU,0x5BU,0x2FU,0xDAU,0x1DU,0x16U,0xE9U,0xA4U,0xF5U,
                    0xBBU,0x57U,0x9AU,0xC2U,0x92U,0x6CU,0x14U,0x62U,0x35U
                };

                // XRP short code 1234567 drops
                uint8_t xrp_short[] = //6198187654261802496
                {
                    0x61U,0x40U,0x00U,0x00U,0x00U,0x00U,0x12U,0xD6U,0x87U
                };

                // XRP long code 755898701447 drops
                uint8_t xrp_long[] = //6294584066823682416
                {
                    0x60U,0x11U,0x40U,0x00U,0x00U,0xAFU,0xFFU,0x12U,0xD6U,0x87U
                };

                // XRP negative 1234567 drops
                uint8_t xrp_neg[] = //1586501635834414592
                {
                    0x61U,0x00U,0x00U,0x00U,0x00U,0x00U,0x12U,0xD6U,0x87U
                };

                // XRP negative zero
                uint8_t xrp_neg_zero[] = // 0
                {
                    0x61U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
                };

                // XRP positive zero
                uint8_t xrp_pos_zero[] = // 0
                {
                    0x61U,0x40U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
                };

 
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);
                    int64_t y;
                    
                    // bounds check
                    ASSERT((y=float_sto_set(1000000, 50)) == OUT_OF_BOUNDS);
                    ASSERT((y=float_sto_set(0, 1000000)) == OUT_OF_BOUNDS);
                    ASSERT((y=float_sto_set(1000000, 1000000)) == OUT_OF_BOUNDS);

                    // too small check
                    ASSERT((y=float_sto_set(0, 7)) == NOT_AN_OBJECT);

                    // garbage check
                    ASSERT((y=float_sto_set(0, 9)) == NOT_AN_OBJECT);
                    ASSERT((y=float_sto_set(0, 8)) == 0);
                

                    ASSERT((y=float_sto_set(SBUF(iou))) == 6198187654261802496ULL);
                    ASSERT((y=float_sto_set(SBUF(xrp_short))) == 6198187654261802496ULL);
                    ASSERT((y=float_sto_set(SBUF(iou_neg))) == 1586501635834414592ULL);
                    ASSERT((y=float_sto_set(SBUF(xrp_neg))) == 1586501635834414592ULL);
                    ASSERT((y=float_sto_set(SBUF(xrp_pos_zero))) == 0);
                    ASSERT((y=float_sto_set(SBUF(xrp_neg_zero))) == 0);
                    ASSERT((y=float_sto_set(SBUF(iou_zero))) == 0);
                    ASSERT((y=float_sto_set(SBUF(xrp_long))) == 6294584066823682416ULL);

                    return accept(0,0,0); 

                }
            )[test.hook]"];

            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set float_sto_set"),
                HSFEE);
            env.close();

            env(pay(bob, alice, XRP(1)), M("test float_sto_set"), fee(XRP(1)));
            env.close();
        }
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
            TestHook hook = wasm[R"[test.hook](
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
                #define float_exponent(f) (((int32_t)(((f) >> 54U) & 0xFFU)) - 97)
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
                    if (diffexp > 1 || diffman > 5000000 || mx < 0 || my < 0)\
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

            env(pay(bob, alice, XRP(1)), M("test float_sum"), fee(XRP(1)));
            env.close();
        }
    }

    void
    test_hook_account()
    {
        testcase("Test hook_account");
        using namespace jtx;

        auto const test = [&](Account alice) -> void {
            Env env{*this, supported_amendments()};

            auto const bob = Account{"bob"};
            env.fund(XRP(10000), alice);
            env.fund(XRP(10000), bob);

            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t hook_account (uint32_t, uint32_t);
                #define TOO_SMALL -4
                #define OUT_OF_BOUNDS -1
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);
                    uint8_t acc[20];

                    // Test out of bounds check
                    ASSERT(hook_account(1000000, 20) == OUT_OF_BOUNDS);
                    ASSERT(hook_account(0, 1000000) == OUT_OF_BOUNDS);
                    ASSERT(hook_account((uint32_t)acc, 19) == TOO_SMALL);
                    ASSERT(hook_account((uint32_t)acc, 20) == 20);

                    // return the accid as the return string
                    accept((uint32_t)acc, 20, 0);
                }
            )[test.hook]"];

            // install the hook on alice
            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set hook_account"),
                HSFEE);
            env.close();

            // invoke the hook
            env(pay(bob, alice, XRP(1)), M("test hook_account"), fee(XRP(1)));

            {
                auto meta = env.meta();

                // ensure hook execution occured
                BEAST_REQUIRE(meta);
                BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

                // ensure there was only one hook execution
                auto const hookExecutions =
                    meta->getFieldArray(sfHookExecutions);
                BEAST_REQUIRE(hookExecutions.size() == 1);

                // get the data in the return string of the extention
                auto const retStr =
                    hookExecutions[0].getFieldVL(sfHookReturnString);

                // check that it matches the account id
                BEAST_EXPECT(retStr.size() == 20);
                auto const a = alice.id();
                BEAST_EXPECT(memcmp(retStr.data(), a.data(), 20) == 0);
            }

            // install the same hook bob
            env(ripple::test::jtx::hook(bob, {{hso(hook, overrideFlag)}}, 0),
                M("set hook_account 2"),
                HSFEE);
            env.close();

            // invoke the hook
            env(pay(bob, alice, XRP(1)), M("test hook_account 2"), fee(XRP(1)));

            // there should be two hook executions, the first should be bob's
            // address the second should be alice's
            {
                auto meta = env.meta();

                // ensure hook execution occured
                BEAST_REQUIRE(meta);
                BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

                // ensure there were two hook executions
                auto const hookExecutions =
                    meta->getFieldArray(sfHookExecutions);
                BEAST_REQUIRE(hookExecutions.size() == 2);

                {
                    // get the data in the return string of the extention
                    auto const retStr =
                        hookExecutions[0].getFieldVL(sfHookReturnString);

                    // check that it matches the account id
                    BEAST_EXPECT(retStr.size() == 20);
                    auto const b = bob.id();
                    BEAST_EXPECT(memcmp(retStr.data(), b.data(), 20) == 0);
                }

                {
                    // get the data in the return string of the extention
                    auto const retStr =
                        hookExecutions[1].getFieldVL(sfHookReturnString);

                    // check that it matches the account id
                    BEAST_EXPECT(retStr.size() == 20);
                    auto const a = alice.id();
                    BEAST_EXPECT(memcmp(retStr.data(), a.data(), 20) == 0);
                }
            }
        };

        test(Account{"alice"});
        test(Account{"cho"});
    }

    void
    test_hook_again()
    {
        testcase("Test hook_again");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_again (void);
            #define PREREQUISITE_NOT_MET	(-9)
            #define ALREADY_SET (-8)
            int64_t hook(uint32_t r)
            {
                _g(1,1);

                if (r > 0)
                {
                    if (hook_again() != PREREQUISITE_NOT_MET)
                        return rollback(0,0,253);

                    return accept(0,0,1);
                }
                
                if (hook_again() != 1)
                    return rollback(0,0,254);
                               
                if (hook_again() != ALREADY_SET)
                    return rollback(0,0,255);
 
                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set hook_again"),
            HSFEE);
        env.close();

        env(pay(bob, alice, XRP(1)), M("test hook_again"), fee(XRP(1)));
        env.close();

        auto meta = env.meta();

        // ensure hook execution occured
        BEAST_REQUIRE(meta);
        BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

        // ensure there were two executions
        auto const hookExecutions =
            meta->getFieldArray(sfHookExecutions);
        BEAST_REQUIRE(hookExecutions.size() == 2);

        // get the data in the return code of the execution
        BEAST_EXPECT(hookExecutions[0].getFieldU64(sfHookReturnCode) == 0);
        BEAST_EXPECT(hookExecutions[1].getFieldU64(sfHookReturnCode) == 1);
        
        // RH TODO: test hook_again on a weak execution not following a strong execution to make sure it fails
    }

    void
    test_hook_hash()
    {
        testcase("Test hook_hash");
        using namespace jtx;

        auto const test = [&](Account alice) -> void {
            Env env{*this, supported_amendments()};

            auto const bob = Account{"bob"};
            env.fund(XRP(10000), alice);
            env.fund(XRP(10000), bob);

            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t hook_hash (uint32_t, uint32_t, int32_t);
                #define TOO_SMALL -4
                #define OUT_OF_BOUNDS -1
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);
                    uint8_t hash[32];

                    // Test out of bounds check
                    ASSERT(hook_hash(1000000, 32, -1) == OUT_OF_BOUNDS);
                    ASSERT(hook_hash((uint32_t)hash, 31, -1) == TOO_SMALL);
                    ASSERT(hook_hash((uint32_t)hash, 32, -1) == 32);

                    // return the hash as the return string
                    accept((uint32_t)hash, 32, 0);
                }
            )[test.hook]"];

            // install the hook on alice
            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set hook_hash"),
                HSFEE);
            env.close();

            // invoke the hook
            env(pay(bob, alice, XRP(1)), M("test hook_hash"), fee(XRP(1)));

            {
                auto meta = env.meta();

                // ensure hook execution occured
                BEAST_REQUIRE(meta);
                BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

                // ensure there was only one hook execution
                auto const hookExecutions =
                    meta->getFieldArray(sfHookExecutions);
                BEAST_REQUIRE(hookExecutions.size() == 1);

                // get the data in the return string of the extention
                auto const retStr =
                    hookExecutions[0].getFieldVL(sfHookReturnString);

                // check that it matches the hook hash
                BEAST_EXPECT(retStr.size() == 32);

                auto const hash = hookExecutions[0].getFieldH256(sfHookHash);
                BEAST_EXPECT(memcmp(hash.data(), retStr.data(), 32) == 0);
            }

            TestHook hook2 = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t hook_hash (uint32_t, uint32_t, int32_t);
                #define TOO_SMALL -4
                #define OUT_OF_BOUNDS -1
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);
                int64_t hook(uint32_t reserved )
                {
                    _g(1,2);
                    uint8_t hash[32];

                    // Test out of bounds check
                    ASSERT(hook_hash(1000000, 32, -1) == OUT_OF_BOUNDS);
                    ASSERT(hook_hash((uint32_t)hash, 31, -1) == TOO_SMALL);
                    ASSERT(hook_hash((uint32_t)hash, 32, -1) == 32);

                    // return the hash as the return string
                    accept((uint32_t)hash, 32, 0);
                }
            )[test.hook]"];

            // install a slightly different hook on bob
            env(ripple::test::jtx::hook(bob, {{hso(hook2, overrideFlag)}}, 0),
                M("set hook_hash 2"),
                HSFEE);
            env.close();

            // invoke the hook
            env(pay(bob, alice, XRP(1)), M("test hook_hash 2"), fee(XRP(1)));

            // there should be two hook executions, the first should have bob's
            // hook hash the second should have alice's hook hash
            {
                auto meta = env.meta();

                // ensure hook execution occured
                BEAST_REQUIRE(meta);
                BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

                // ensure there was only one hook execution
                auto const hookExecutions =
                    meta->getFieldArray(sfHookExecutions);
                BEAST_REQUIRE(hookExecutions.size() == 2);

                // get the data in the return string of the extention
                auto const retStr1 =
                    hookExecutions[0].getFieldVL(sfHookReturnString);

                // check that it matches the hook hash
                BEAST_EXPECT(retStr1.size() == 32);

                auto const hash1 = hookExecutions[0].getFieldH256(sfHookHash);
                BEAST_EXPECT(memcmp(hash1.data(), retStr1.data(), 32) == 0);

                // get the data in the return string of the extention
                auto const retStr2 =
                    hookExecutions[1].getFieldVL(sfHookReturnString);

                // check that it matches the hook hash
                BEAST_EXPECT(retStr2.size() == 32);

                auto const hash2 = hookExecutions[1].getFieldH256(sfHookHash);
                BEAST_EXPECT(memcmp(hash2.data(), retStr2.data(), 32) == 0);

                // make sure they're not the same
                BEAST_EXPECT(memcmp(hash1.data(), hash2.data(), 32) != 0);

                // compute the hashes
                auto computedHash2 = ripple::sha512Half_s(
                    ripple::Slice(hook.data(), hook.size()));

                auto computedHash1 = ripple::sha512Half_s(
                    ripple::Slice(hook2.data(), hook2.size()));

                // ensure the computed hashes match
                BEAST_EXPECT(computedHash1 == hash1);
                BEAST_EXPECT(computedHash2 == hash2);
            }
        };

        test(Account{"alice"});
    }

    void
    test_hook_param()
    {
        testcase("Test hook_param");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_param(uint32_t, uint32_t, uint32_t, uint32_t);
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define OUT_OF_BOUNDS (-1)
            #define TOO_BIG (-3)
            #define TOO_SMALL (-4)
            #define DOESNT_EXIST (-5)
            uint8_t* names[] =
            {
                "param0",
                "param1",
                "param2",
                "param3",
                "param4",
                "param5",
                "param6",
                "param7",
                "param8",
                "param9",
                "param10",
                "param11",
                "param12",
                "param13",
                "param14",
                "param15"
            };
            uint8_t* values[] =
            {
                "value0",
                "value1",
                "value2",
                "value3",
                "value4",
                "value5",
                "value6",
                "value7",
                "value8",
                "value9",
                "value10",
                "value11",
                "value12",
                "value13",
                "value14",
                "value15"
            };
            #define SBUF(x) x,sizeof(x)
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                ASSERT(hook_param(0, 1000000, 0,32) == OUT_OF_BOUNDS);
                ASSERT(hook_param(1000000, 32, 0, 32) == OUT_OF_BOUNDS);
                ASSERT(hook_param(0, 32, 1000000, 32) == OUT_OF_BOUNDS);
                ASSERT(hook_param(0, 32, 0, 1000000) == OUT_OF_BOUNDS);
                ASSERT(hook_param(0, 32, 0, 33) == TOO_BIG);
                ASSERT(hook_param(0, 32, 0, 0) == TOO_SMALL);
                ASSERT(hook_param(0, 32, 0, 32) == DOESNT_EXIST);

                uint8_t buf[32];

                for (int i = 0; GUARD(16), i < 16; ++i)
                {
                    int s = 6 + (i < 10 ? 0 : 1);
                    int64_t v = hook_param(SBUF(buf), names[i], s);
                    ASSERT(v == s);

                    ASSERT(buf[0] == 'v' && buf[1] == 'a' && buf[2] == 'l' && buf[3] == 'u' && buf[4] == 'e');
                    ASSERT(*(buf + v - 1) == *(values[i] + v - 1));
                    ASSERT(*(buf + v - 2) == *(values[i] + v - 2));
                }
                
                accept(0,0,0);
            }
            
        )[test.hook]"];

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        Json::Value iv;
        iv[jss::CreateCode] = strHex(hook);
        iv[jss::HookOn] = uint64_hex(0);
        iv[jss::HookApiVersion] = 0U;
        iv[jss::HookNamespace] =  to_string(uint256{beast::zero});
        Json::Value params{Json::arrayValue};
        for (uint32_t i = 0; i < 16; ++i)
        {
            Json::Value pv;
            Json::Value piv;
            piv[jss::HookParameterName] =
                strHex("param" + std::to_string(i));
            piv[jss::HookParameterValue] =
                strHex("value" + std::to_string(i));
            pv[jss::HookParameter] = piv;
            params[i] = pv;
        }
        iv[jss::HookParameters] = params;
        jv[jss::Hooks][0U][jss::Hook] = iv;
        env(jv,
            M("set hook_param"),
            HSFEE,
            ter(tesSUCCESS));
        env.close();

        // invoke
        env(pay(bob, alice, XRP(1)), M("test hook_param"), fee(XRP(1)));
        env.close();

    }

    void
    test_hook_param_set()
    {
        testcase("Test hook_param_set");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);
        
        TestHook checker_wasm = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);

            extern int64_t hook_param(uint32_t, uint32_t, uint32_t, uint32_t);
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define OUT_OF_BOUNDS (-1)
            #define TOO_BIG (-3)
            #define TOO_SMALL (-4)
            #define DOESNT_EXIST (-5)
            #define INVALID_ARGUMENT (-7)
            uint8_t* names[] =
            {
                "param0",
                "param1",
                "param2",
                "param3",
            };
            #define SBUF(x) x,sizeof(x)
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                // this entry should be deleted by the setter
                uint8_t checker_hash[32];
                ASSERT(hook_param(SBUF(checker_hash), "checker", 7) == DOESNT_EXIST);

                
                uint8_t buf[32];

                // this entry should havebeen added by the setter
                ASSERT(hook_param(SBUF(buf), "hello", 5) == 5);
                ASSERT(buf[0] == 'w' && buf[1] == 'o' && buf[2] == 'r' && buf[3] == 'l' && buf[4] == 'd');

                // these pre-existing entries should be modified by the setter
                for (int i = 0; GUARD(4), i < 4; ++i)
                {
                    ASSERT(hook_param(SBUF(buf), names[i], 6) == 6);

                    ASSERT(buf[0] == 'v' && buf[1] == 'a' && buf[2] == 'l' &&
                        buf[3] == 'u' && buf[4] == 'e' && buf[5] == '0' +  i);
                
                }
                
                accept(0,0,0);
            }
            
        )[test.hook]"];

        TestHook setter_wasm = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_param (uint32_t, uint32_t, uint32_t, uint32_t);
            extern int64_t hook_param_set (
                uint32_t read_ptr,
                uint32_t read_len,
                uint32_t kread_ptr,
                uint32_t kread_len,
                uint32_t hread_ptr,
                uint32_t hread_len  
            );
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define OUT_OF_BOUNDS (-1)
            #define TOO_BIG (-3)
            #define TOO_SMALL (-4)
            #define DOESNT_EXIST (-5)
            #define INVALID_ARGUMENT (-7)
            uint8_t* names[] =
            {
                "param0",
                "param1",
                "param2",
                "param3",
            };
            uint8_t* values[] =
            {
                "value0",
                "value1",
                "value2",
                "value3",
            };
            #define SBUF(x) x,sizeof(x)
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);


                ASSERT(hook_param_set(1000000, 32, 0, 32, 0, 32) == OUT_OF_BOUNDS);
                ASSERT(hook_param_set(0, 1000000, 0, 32, 0, 32) == OUT_OF_BOUNDS);
                ASSERT(hook_param_set(0, 32, 1000000, 32, 0, 32) == OUT_OF_BOUNDS);
                ASSERT(hook_param_set(0, 32, 0, 1000000, 0, 32) == OUT_OF_BOUNDS);
                ASSERT(hook_param_set(0, 32, 0, 32, 1000000, 32) == OUT_OF_BOUNDS);
                ASSERT(hook_param_set(0, 32, 0, 32, 0, 1000000) == OUT_OF_BOUNDS);
                ASSERT(hook_param_set(0, 32, 0, 0, 0, 32) == TOO_SMALL);
                ASSERT(hook_param_set(0, 32, 0, 33, 0, 32) == TOO_BIG);
                ASSERT(hook_param_set(0, 32, 0, 32, 0, 33) == INVALID_ARGUMENT);
                ASSERT(hook_param_set(0, 32, 0, 32, 0, 31) == INVALID_ARGUMENT);
                ASSERT(hook_param_set(0, 32, 0, 32, 0, 0) == INVALID_ARGUMENT);
                ASSERT(hook_param_set(0, 257, 0, 32, 0, 32) == TOO_BIG);


                uint8_t checker_hash[32];
                ASSERT(hook_param(SBUF(checker_hash), "checker", 7) == 32);

                for (int i = 0; GUARD(4), i < 4; ++i)
                {
                    ASSERT(hook_param_set(values[i], 6, names[i], 6, SBUF(checker_hash)) == 6);
                }
                
                // "delete" the checker entry" for when the checker runs
                ASSERT(hook_param_set(0,0,"checker", 7, SBUF(checker_hash)) == 0);

                // add a parameter that did not previously exist
                ASSERT(hook_param_set("world", 5,"hello", 5, SBUF(checker_hash)) == 5);

                // ensure this hook's parameters did not change
                uint8_t buf[32];
                for (int i = 0; GUARD(4), i < 4; ++i)
                {
                    ASSERT(hook_param(SBUF(buf), names[i], 6) == 6);

                    ASSERT(buf[0] == 'v' && buf[1] == 'a' && buf[2] == 'l' &&
                        buf[3] == 'u' && buf[4] == 'e' && buf[5] == '0');
                
                }
 
                accept(0,0,0);
            }
            
        )[test.hook]"];

        HASH_WASM(checker);

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        Json::Value iv;
        iv[jss::CreateCode] = strHex(setter_wasm);
        iv[jss::HookOn] = uint64_hex(0);
        iv[jss::HookApiVersion] = 0U;
        iv[jss::HookNamespace] =  to_string(uint256{beast::zero});

        Json::Value checkerpv;
        {
            Json::Value piv;
            piv[jss::HookParameterName] = 
                strHex(std::string("checker"));
            piv[jss::HookParameterValue] =
                checker_hash_str;
            checkerpv[jss::HookParameter] = piv;
        }

        Json::Value params{Json::arrayValue};
        for (uint32_t i = 0; i < 4; ++i)
        {
            Json::Value pv;
            Json::Value piv;
            piv[jss::HookParameterName] =
                strHex("param" + std::to_string(i));
            piv[jss::HookParameterValue] =
                strHex(std::string("value0"));
            pv[jss::HookParameter] = piv;
            params[i] = pv;
        }
        params[4U] = checkerpv;        

        iv[jss::HookParameters] = params;
        jv[jss::Hooks][0U][jss::Hook] = iv;

        {
            iv[jss::CreateCode] = strHex(checker_wasm);
            Json::Value params{Json::arrayValue};
            params[0U] = checkerpv;
            iv[jss::HookParameters] = params;
            jv[jss::Hooks][3U][jss::Hook] = iv;
            jv[jss::Hooks][1U][jss::Hook] = Json::objectValue;
            jv[jss::Hooks][2U][jss::Hook] = Json::objectValue;
        }
   
        env(jv,
            M("set hook_param_set"),
            HSFEE,
            ter(tesSUCCESS));
        env.close();

        // invoke
        env(pay(bob, alice, XRP(1)), M("test hook_param_set"), fee(XRP(1)));
        env.close();

    }

    void
    test_hook_pos()
    {
        testcase("Test hook_pos");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_pos (void);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                accept(0,0,hook_pos());
            }
        )[test.hook]"];

        // install the hook on alice in all four spots
        env(ripple::test::jtx::hook(
                alice,
                {{hso(hook),
                  hso(hook),
                  hso(hook),
                  hso(hook)}},
                0),
            M("set hook_pos"),
            HSFEE,
            ter(tesSUCCESS));
        env.close();

        // invoke the hooks
        env(pay(bob, alice, XRP(1)), M("test hook_pos"), fee(XRP(1)));
        env.close();

        auto meta = env.meta();

        // ensure hook execution occured
        BEAST_REQUIRE(meta);
        BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

        // ensure there was four hook executions
        auto const hookExecutions =
            meta->getFieldArray(sfHookExecutions);
        BEAST_REQUIRE(hookExecutions.size() == 4);

        // get the data in the return code of the execution
        BEAST_EXPECT(hookExecutions[0].getFieldU64(sfHookReturnCode) == 0);
        BEAST_EXPECT(hookExecutions[1].getFieldU64(sfHookReturnCode) == 1);
        BEAST_EXPECT(hookExecutions[2].getFieldU64(sfHookReturnCode) == 2);
        BEAST_EXPECT(hookExecutions[3].getFieldU64(sfHookReturnCode) == 3);

    }

    void
    test_hook_skip()
    {
        testcase("Test hook_skip");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook skip_wasm = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_skip (uint32_t, uint32_t, uint32_t);
            extern int64_t otxn_field (uint32_t, uint32_t, uint32_t);
            extern int64_t hook_hash (uint32_t, uint32_t, int32_t);
            extern int64_t hook_pos(void);
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__ << 8U);
            #define sfInvoiceID ((5U << 16U) + 17U)
            #define SBUF(x) x,sizeof(x)
            #define OUT_OF_BOUNDS (-1)
            #define DOESNT_EXIST (-5)
            #define INVALID_ARGUMENT (-7)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                // bounds checks
                ASSERT(hook_skip(0, 1000000, 0) == OUT_OF_BOUNDS);
                ASSERT(hook_skip(1000000, 32, 0) == OUT_OF_BOUNDS);
                ASSERT(hook_skip(1000000, 100000, 0) == OUT_OF_BOUNDS);
                ASSERT(hook_skip(0, 33, 0) == INVALID_ARGUMENT);
                ASSERT(hook_skip(0, 1000000, 1) == OUT_OF_BOUNDS);
                ASSERT(hook_skip(1000000, 32, 1) == OUT_OF_BOUNDS);
                ASSERT(hook_skip(1000000, 100000, 1) == OUT_OF_BOUNDS);
                ASSERT(hook_skip(0, 33, 1) == INVALID_ARGUMENT);

                // garbage check
                ASSERT(hook_skip(0, 32, 0) == DOESNT_EXIST);
                ASSERT(hook_skip(0, 32, 1) == DOESNT_EXIST);
                ASSERT(hook_skip(0, 32, 2) == INVALID_ARGUMENT);

                // the hook to skip is passed in by invoice id
                uint8_t skip[32];
                ASSERT(otxn_field(SBUF(skip), sfInvoiceID) == 32);

                // get this hook's hash
                uint8_t hash[32];
                ASSERT(hook_hash(SBUF(hash), (uint32_t)hook_pos()) == 32);

                // to test if the "remove" function works in the api we will add this hook hash itself and then
                // remove it again. Therefore if the hook is placed at positions 0 and 3, the one at 3 should still
                // run
                ASSERT(hook_skip(SBUF(hash), 1) == DOESNT_EXIST);
                ASSERT(hook_skip(SBUF(hash), 0) == 1);
                ASSERT(hook_skip(SBUF(hash), 1) == 1);

                // finally skip the hook hash indicated by invoice id
                ASSERT(hook_skip(SBUF(skip), 0));

                accept(0,0,hook_pos());
            }

        )[test.hook]"];


        TestHook pos_wasm = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_pos (void);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                accept(0,0,255);
            }
        )[test.hook]"];
        
        HASH_WASM(pos);

        // install the hook on alice in one places
        env(ripple::test::jtx::hook(
                alice,
                {{hso(skip_wasm),
                  hso(pos_wasm),
                  hso(pos_wasm),
                  hso(skip_wasm)}},
                0),
            M("set hook_skip"),
            HSFEE,
            ter(tesSUCCESS));
        env.close();


        // invoke the hooks
        {
            Json::Value json = pay(bob, alice, XRP(1));
            json[jss::InvoiceID] = pos_hash_str;
            env(json, fee(XRP(1)), M("test hook_skip"), ter(tesSUCCESS));
            env.close();
        }

        auto meta = env.meta();

        // ensure hook execution occured
        BEAST_REQUIRE(meta);
        BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

        // ensure there was four hook executions
        auto const hookExecutions =
            meta->getFieldArray(sfHookExecutions);
        BEAST_REQUIRE(hookExecutions.size() == 2);

        // get the data in the return code of the execution
        BEAST_EXPECT(hookExecutions[0].getFieldU64(sfHookReturnCode) == 0);
        BEAST_EXPECT(hookExecutions[1].getFieldU64(sfHookReturnCode) == 3);
    }

    void
    test_ledger_keylet()
    {
        testcase("Test ledger_keylet");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t ledger_keylet(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
            extern int64_t slot_set(uint32_t, uint32_t, uint32_t);
            #define ASSERT(x)\
            if (!(x))\
            {\
                rollback((uint32_t)#x, sizeof(#x), __LINE__);\
            }
            #define SBUF(x) x, sizeof(x)
            #define OUT_OF_BOUNDS (-1)
            #define TOO_SMALL (-4)
            #define TOO_BIG (-3)
            #define INVALID_ARGUMENT (-7)
            #define DOESNT_EXIST (-5)
            #define DOES_NOT_MATCH (-40)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
    
                ASSERT(ledger_keylet(1000000, 34, 0, 34, 0, 34) == OUT_OF_BOUNDS);
                ASSERT(ledger_keylet(0, 1000000, 0, 34, 0, 34) == OUT_OF_BOUNDS);
                ASSERT(ledger_keylet(0, 34, 1000000, 34, 0, 34) == OUT_OF_BOUNDS);
                ASSERT(ledger_keylet(0, 34, 0, 1000000, 0, 34) == OUT_OF_BOUNDS);
                ASSERT(ledger_keylet(0, 34, 0, 34, 1000000, 34) == OUT_OF_BOUNDS);
                ASSERT(ledger_keylet(0, 34, 0, 34, 0, 1000000) == OUT_OF_BOUNDS);

                ASSERT(ledger_keylet(0, 33, 0, 34, 0, 34) == TOO_SMALL);
                ASSERT(ledger_keylet(0, 34, 0, 33, 0, 34) == TOO_SMALL);
                ASSERT(ledger_keylet(0, 34, 0, 34, 0, 33) == TOO_SMALL);

                ASSERT(ledger_keylet(0, 35, 0, 34, 0, 34) == TOO_BIG);
                ASSERT(ledger_keylet(0, 34, 0, 35, 0, 34) == TOO_BIG);
                ASSERT(ledger_keylet(0, 34, 0, 34, 0, 35) == TOO_BIG);
            
                uint8_t trash[34] = {
                    1,2,
                    1, 2, 3, 4, 5, 6, 7, 8, 9,
                    1, 2, 3, 4, 5, 6, 7, 8, 9,
                    1, 2, 3, 4, 5, 6, 7, 8, 9,
                    1, 2, 3, 4, 5, 6, 7, 8, 9
                };
                
                uint8_t trash2[34] = {
                    1,2,
                    1, 2, 3, 4, 5, 6, 7, 8, 9,
                    1, 2, 3, 4, 5, 6, 7, 8, 9,
                    1, 2, 3, 4, 5, 6, 7, 8, 9,
                    1, 2, 3, 4, 5, 6, 7, 8, 10
                };
                
                
                ASSERT(ledger_keylet(0, 34, SBUF(trash2), SBUF(trash)) == DOESNT_EXIST);
                ASSERT(ledger_keylet(0, 34, SBUF(trash), SBUF(trash2)) == DOESNT_EXIST);


                uint8_t first[34];
                uint8_t last[34] = {
                    0x00U, 0x01U,
                    0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
                    0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
                    0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
                    0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFEU
                };

                uint8_t out[34];
                ASSERT(ledger_keylet(SBUF(out), SBUF(first), SBUF(last)) == DOES_NOT_MATCH);
                last[1] = 0;
                ASSERT(ledger_keylet(SBUF(out), SBUF(first), SBUF(last)) == 34);

                ASSERT(slot_set(SBUF(out), 1) == 1);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set ledger_keylet"),
            HSFEE);
        env.close();

        env(pay(bob, alice, XRP(1)), M("test ledger_keylet"), fee(XRP(1)));
        env.close();
    }

    void
    test_ledger_last_hash()
    {
        testcase("Test ledger_last_hash");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t ledger_last_hash (uint32_t, uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t hash[32];

                // Test out of bounds check
                ASSERT(ledger_last_hash(1000000, 32) == OUT_OF_BOUNDS);
                ASSERT(ledger_last_hash((uint32_t)hash, 31) == TOO_SMALL);
                ASSERT(ledger_last_hash((uint32_t)hash, 32) == 32);

                // return the hash
                accept((uint32_t)hash, 32, 0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set ledger_last_hash"),
            HSFEE);
        env.close();

        for (uint32_t i = 0; i < 3; ++i)
        {
            auto const llh = env.app().getLedgerMaster().getClosedLedger()->info().hash;

            env(pay(bob, alice, XRP(1)), M("test ledger_last_hash"), fee(XRP(1)));

            auto meta = env.meta();

            // ensure hook execution occured
            BEAST_REQUIRE(meta);
            BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

            // ensure there was only one hook execution
            auto const hookExecutions =
                meta->getFieldArray(sfHookExecutions);
            BEAST_REQUIRE(hookExecutions.size() == 1);

            // get the data in the return string of the extention
            auto const retStr =
                hookExecutions[0].getFieldVL(sfHookReturnString);

            // check that it matches the expected size (32 bytes)
            BEAST_EXPECT(retStr.size() == 32);

            BEAST_EXPECT(llh == uint256::fromVoid(retStr.data()));
        }
    }

    void
    test_ledger_last_time()
    {
        testcase("Test ledger_last_time");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t ledger_last_time (void);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                accept(0,0,ledger_last_time());
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set ledger_last_time"),
            HSFEE);
        env.close();

        // invoke the hook a few times
        for (uint32_t i = 0; i < 3; ++i)
        {
            int64_t llc = 
            std::chrono::duration_cast<std::chrono::seconds>
            (
                env.app().getLedgerMaster()
                    .getValidatedLedger()->info()
                        .parentCloseTime
                            .time_since_epoch()
            ).count();

            env(pay(bob, alice, XRP(1)), M("test ledger_last_time"), fee(XRP(1)));
            env.close();
            {
                auto meta = env.meta();

                // ensure hook execution occured
                BEAST_REQUIRE(meta);
                BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

                // ensure there was only one hook execution
                auto const hookExecutions =
                    meta->getFieldArray(sfHookExecutions);
                BEAST_REQUIRE(hookExecutions.size() == 1);

                // get the data in the return code of the execution
                auto const rc =
                    hookExecutions[0].getFieldU64(sfHookReturnCode);

                // check that it matches the last ledger seq number
                BEAST_EXPECT(llc == rc);
            }
        }

    }

    void
    test_ledger_nonce()
    {
        testcase("Test ledger_nonce");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t ledger_nonce (uint32_t, uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t nonce[64];

                // Test out of bounds check
                ASSERT(ledger_nonce(1000000, 32) == OUT_OF_BOUNDS);
                ASSERT(ledger_nonce((uint32_t)nonce, 31) == TOO_SMALL);
                ASSERT(ledger_nonce((uint32_t)nonce, 32) == 32);
                ASSERT(ledger_nonce((uint32_t)(nonce + 32), 32) == 32);

                // return the two nonces as the return string
                accept((uint32_t)nonce, 64, 0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set ledger_nonce"),
            HSFEE);
        env.close();



        // invoke the hook
        auto const seq = env.app().getLedgerMaster().getCurrentLedger()->info().seq;
        auto const llc = env.app().getLedgerMaster().getCurrentLedger()->info().parentCloseTime.time_since_epoch().count();
        auto const llh = env.app().getLedgerMaster().getCurrentLedger()->info().hash;

        env(pay(bob, alice, XRP(1)), M("test ledger_nonce"), fee(XRP(1)));
        auto const txid = env.txid();

        auto meta = env.meta();

        // ensure hook execution occured
        BEAST_REQUIRE(meta);
        BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

        // ensure there was only one hook execution
        auto const hookExecutions =
            meta->getFieldArray(sfHookExecutions);
        BEAST_REQUIRE(hookExecutions.size() == 1);

        // get the data in the return string of the extention
        auto const retStr =
            hookExecutions[0].getFieldVL(sfHookReturnString);

        // check that it matches the expected size (two nonces = 64 bytes)
        BEAST_EXPECT(retStr.size() == 64);

        auto const computed_hash_1 = ripple::sha512Half(
            ripple::HashPrefix::hookNonce,
            seq, llc, llh,
            txid,
            (uint16_t)0UL,
            alice.id()
        );
        auto const computed_hash_2 = ripple::sha512Half(
            ripple::HashPrefix::hookNonce,
            seq, llc, llh,
            txid,
            (uint16_t)1UL,              // second nonce
            alice.id()
        );

        /*
        std::cout << 
            "seq: " << seq << 
            ", llc: " << llc << 
            ", llh: " << llh << 
            ", txid: " << txid << 
            ", count: 0" << 
            ", acc: " << alice.human() <<
            ", nonce: " << computed_hash_1 << ", " << computed_hash_2 << "\n";
        */

        BEAST_EXPECT(computed_hash_1 == uint256::fromVoid(retStr.data()));
        BEAST_EXPECT(computed_hash_2 == uint256::fromVoid(retStr.data() + 32));

    }

    void
    test_ledger_seq()
    {
        testcase("Test ledger_seq");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t ledger_seq (void);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                accept(0,0,ledger_seq());
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set ledger_seq"),
            HSFEE);
        env.close();

        // invoke the hook a few times
        for (uint32_t i = 0; i < 3; ++i)
        {
            env(pay(bob, alice, XRP(1)), M("test ledger_seq"), fee(XRP(1)));
            env.close();
            {
                auto meta = env.meta();

                // ensure hook execution occured
                BEAST_REQUIRE(meta);
                BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

                // ensure there was only one hook execution
                auto const hookExecutions =
                    meta->getFieldArray(sfHookExecutions);
                BEAST_REQUIRE(hookExecutions.size() == 1);

                // get the data in the return code of the execution
                auto const rc =
                    hookExecutions[0].getFieldU64(sfHookReturnCode);

                // check that it matches the last ledger seq number
                BEAST_EXPECT(env.app().getLedgerMaster().getClosedLedger()->info().seq == rc);
            }
        }

    }

    void
    test_meta_slot()
    {
        testcase("Test meta_slot");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_again (void);
            extern int64_t slot(uint32_t, uint32_t, uint32_t);
            extern int64_t trace(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
            extern int64_t meta_slot(uint32_t);
             extern int64_t slot_subfield (
                uint32_t parent_slot,
                uint32_t field_id,
                uint32_t new_slot
            );
            #define PREREQUISITE_NOT_MET	(-9)
            #define ALREADY_SET (-8)
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define sfHookExecutions ((15U << 16U) + 18U)
            #define sfTransactionResult ((16U << 16U) + 3U)
            #define sfAffectedNodes ((15U << 16U) + 8U)
            #define sfTransactionIndex ((2U << 16U) + 28U)
            int64_t hook(uint32_t r)
            {
                _g(1,1);

                if (r > 0)
                {
                    ASSERT(meta_slot(1) == 1);

                    uint8_t buf[1024];
                    ASSERT(slot(buf, 1024, 1) > 200);

                    ASSERT(slot_subfield(1, sfTransactionIndex, 2) == 2);
                    ASSERT(slot_subfield(1, sfAffectedNodes, 3) == 3);
                    ASSERT(slot_subfield(1, sfHookExecutions, 4) == 4);
                    ASSERT(slot_subfield(1, sfTransactionResult, 5) == 5);
                    
                    return accept(0,0,1);
                }
                
                if (hook_again() != 1)
                    return rollback(0,0,254);
                               
                ASSERT(meta_slot(1) == PREREQUISITE_NOT_MET);

                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set meta_slot"),
            HSFEE);
        env.close();

        env(pay(bob, alice, XRP(1)), M("test meta_slot"), fee(XRP(1)));
        env.close();

        auto meta = env.meta();

        // ensure hook execution occured
        BEAST_REQUIRE(meta);
        BEAST_REQUIRE(meta->isFieldPresent(sfHookExecutions));

        // ensure there were two executions
        auto const hookExecutions =
            meta->getFieldArray(sfHookExecutions);
        BEAST_REQUIRE(hookExecutions.size() == 2);

        // get the data in the return code of the execution
        BEAST_EXPECT(hookExecutions[0].getFieldU64(sfHookReturnCode) == 0);
        BEAST_EXPECT(hookExecutions[1].getFieldU64(sfHookReturnCode) == 1);
        
    }

    void
    test_otxn_field()
    {
        testcase("Test otxn_field");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t otxn_field (uint32_t write_ptr, uint32_t write_len, uint32_t sfcode);
            extern int64_t hook_account(uint32_t, uint32_t);
            #define OUT_OF_BOUNDS -1
            #define TOO_BIG -3
            #define TOO_SMALL -4
            #define INVALID_ARGUMENT -7
            #define INVALID_FIELD -17
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            #define sfAccount ((8U << 16U) + 1U)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                // bounds check
                ASSERT(otxn_field(1, 1000000, sfAccount) == OUT_OF_BOUNDS);
                ASSERT(otxn_field(1000000, 20, sfAccount) == OUT_OF_BOUNDS);

                // sanity check
                ASSERT(otxn_field(0, 1, sfAccount) == INVALID_ARGUMENT);
                
                // size check
                ASSERT(otxn_field(0, 0, sfAccount) == TOO_BIG);
            
                uint8_t acc[20];
                ASSERT(otxn_field(acc, 19, sfAccount) == TOO_SMALL);

                ASSERT(otxn_field(acc, 20, sfAccount) == 20);

                ASSERT(otxn_field(acc, 20, 1) == INVALID_FIELD);
                uint8_t acc2[20];
                ASSERT(hook_account(acc2, 20) == 20);

                for (int i = 0; GUARD(20), i < 20; ++i)
                    ASSERT(acc[i] == acc2[i]);
                
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set otxn_field"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(alice, bob, XRP(1)), M("test otxn_field"), fee(XRP(1)));
    }

    void
    test_otxn_id()
    {
        testcase("Test otxn_id");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot (uint32_t write_ptr, uint32_t write_len, uint32_t slot_no);
            extern int64_t otxn_id (uint32_t write_ptr, uint32_t write_len, uint32_t flags);
            extern int64_t util_sha512h(uint32_t write_ptr, uint32_t write_len,
                                    uint32_t read_ptr,  uint32_t read_len );
            extern int64_t otxn_slot (
              uint32_t slot_no
            );
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                // bounds check
                ASSERT(otxn_id(1, 1000000, 0) == OUT_OF_BOUNDS);
                ASSERT(otxn_id(1000000, 1024, 0) == OUT_OF_BOUNDS);

                // size check
                ASSERT(otxn_id(1, 0, 0) == TOO_SMALL);
                ASSERT(otxn_id(1, 31, 0) == TOO_SMALL);
              
                uint8_t id[32];
                ASSERT(otxn_id(SBUF(id), 0) == 32);
 
                // slot the otxn then generate a canonical hash over it 
                ASSERT(otxn_slot(1) == 1);

                uint8_t buf[1024];
                int64_t size = slot(buf + 4, sizeof(buf) - 4, 1);

                
                ASSERT(size > 0);

                buf[0] = 'T';
                buf[1] = 'X';
                buf[2] = 'N';
                buf[3] = 0;

                uint8_t hash[32];
                ASSERT(util_sha512h(SBUF(hash), buf, size+4) == 32);

                for (int i = 0; GUARD(32), i < 32; ++i)
                    ASSERT(hash[i] == id[i]);

                // RH TODO: test the flags = 1 on emitted txn

                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set otxn_id"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test otxn_id"), fee(XRP(1)));
    }

    void
    test_otxn_slot()
    {
        testcase("Test otxn_slot");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot (uint32_t write_ptr, uint32_t write_len, uint32_t slot_no);
            extern int64_t otxn_id (uint32_t write_ptr, uint32_t write_len, uint32_t flags);
            extern int64_t util_sha512h(uint32_t write_ptr, uint32_t write_len,
                                    uint32_t read_ptr,  uint32_t read_len );
            extern int64_t otxn_slot (
              uint32_t slot_no
            );
            #define INVALID_ARGUMENT -7
            #define NO_FREE_SLOTS -6
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                ASSERT(otxn_slot(256) == INVALID_ARGUMENT);

                ASSERT(otxn_slot(1) == 1);

                uint8_t id[32];
                ASSERT(otxn_id(SBUF(id), 0) == 32);
 
                // slot the otxn then generate a canonical hash over it 
                ASSERT(otxn_slot(1) == 1);

                uint8_t buf[1024];
                int64_t size = slot(buf + 4, sizeof(buf) - 4, 1);
                
                ASSERT(size > 0);

                buf[0] = 'T';
                buf[1] = 'X';
                buf[2] = 'N';
                buf[3] = 0;

                uint8_t hash[32];
                ASSERT(util_sha512h(SBUF(hash), buf, size+4) == 32);

                for (int i = 0; GUARD(32), i < 32; ++i)
                    ASSERT(hash[i] == id[i]);
                
                // slot exhaustion
                for (int i = 0; GUARD(255), i < 254; ++i)
                    ASSERT(otxn_slot(0) > 0);
    
                ASSERT(otxn_slot(0) == NO_FREE_SLOTS);

                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set otxn_slot"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test otxn_slot"), fee(XRP(1)));
    }

    void
    test_otxn_type()
    {
        testcase("Test otxn_type");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot (uint32_t write_ptr, uint32_t write_len, uint32_t slot_no);
            extern int64_t otxn_slot (
              uint32_t slot_no
            );
            extern int64_t slot_subfield(
                uint32_t parent_slot,
                uint32_t field_id,
                uint32_t new_slot
            );
            extern int64_t otxn_type(void);
            #define sfTransactionType ((1U << 16U) + 2U)
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                ASSERT(otxn_slot(1) == 1);
            
                ASSERT(slot_subfield(1, sfTransactionType, 2) == 2);
        
                int64_t tt = slot(0,0,2);

                ASSERT(tt == otxn_type());

                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set otxn_type"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test otxn_type"), fee(XRP(1)));

        // invoke it another way
        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::AccountSet;
        jv[jss::Flags] = 0;

        // invoke the hook
        env(jv, M("test otxn_type 2"), fee(XRP(1)));
        
        // RH TODO: test behaviour on emit failure
    }

    void
    test_otxn_param()
    {
        testcase("Test otxn_param");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t otxn_param(uint32_t, uint32_t, uint32_t, uint32_t);
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define OUT_OF_BOUNDS (-1)
            #define TOO_BIG (-3)
            #define TOO_SMALL (-4)
            #define DOESNT_EXIST (-5)
            uint8_t* names[] =
            {
                "param0",
                "param1",
                "param2",
                "param3",
                "param4",
                "param5",
                "param6",
                "param7",
                "param8",
                "param9",
                "param10",
                "param11",
                "param12",
                "param13",
                "param14",
                "param15"
            };
            uint8_t* values[] =
            {
                "value0",
                "value1",
                "value2",
                "value3",
                "value4",
                "value5",
                "value6",
                "value7",
                "value8",
                "value9",
                "value10",
                "value11",
                "value12",
                "value13",
                "value14",
                "value15"
            };
            #define SBUF(x) x,sizeof(x)
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                ASSERT(otxn_param(0, 1000000, 0,32) == OUT_OF_BOUNDS);
                ASSERT(otxn_param(1000000, 32, 0, 32) == OUT_OF_BOUNDS);
                ASSERT(otxn_param(0, 32, 1000000, 32) == OUT_OF_BOUNDS);
                ASSERT(otxn_param(0, 32, 0, 1000000) == OUT_OF_BOUNDS);
                ASSERT(otxn_param(0, 32, 0, 33) == TOO_BIG);
                ASSERT(otxn_param(0, 32, 0, 0) == TOO_SMALL);
                ASSERT(otxn_param(0, 32, 0, 32) == DOESNT_EXIST);

                uint8_t buf[32];

                for (int i = 0; GUARD(16), i < 16; ++i)
                {
                    int s = 6 + (i < 10 ? 0 : 1);
                    int64_t v = otxn_param(SBUF(buf), names[i], s);
                    ASSERT(v == s);

                    ASSERT(buf[0] == 'v' && buf[1] == 'a' && buf[2] == 'l' && buf[3] == 'u' && buf[4] == 'e');
                    ASSERT(*(buf + v - 1) == *(values[i] + v - 1));
                    ASSERT(*(buf + v - 2) == *(values[i] + v - 2));
                }
                
                accept(0,0,0);
            }
            
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set otxn_param"),
            HSFEE);
        env.close();

        // invoke
        Json::Value invoke;
        invoke[jss::TransactionType] = "Invoke";
        invoke[jss::Account] = bob.human();
        invoke[jss::Destination] = alice.human();

        Json::Value params{Json::arrayValue};
        for (uint32_t i = 0; i < 16; ++i)
        {
            Json::Value pv;
            Json::Value piv;
            piv[jss::HookParameterName] =
                strHex("param" + std::to_string(i));
            piv[jss::HookParameterValue] =
                strHex("value" + std::to_string(i));
            pv[jss::HookParameter] = piv;
            params[i] = pv;
        }
        invoke[jss::HookParameters] = params;

        env(invoke, M("test otxn_param"), fee(XRP(1)));
        env.close();
    }
    
    void
    test_slot()
    {
        testcase("Test slot");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot (uint32_t write_ptr, uint32_t write_len, uint32_t slot_no);
            extern int64_t slot_subfield (
                uint32_t parent_slot,
                uint32_t field_id,
                uint32_t new_slot
            );
            extern int64_t util_keylet (
                uint32_t write_ptr, uint32_t write_len, uint32_t keylet_type,
                uint32_t a, uint32_t b, uint32_t c,
                uint32_t d, uint32_t e, uint32_t f
            );
            extern int64_t sto_subfield(uint32_t, uint32_t, uint32_t);
            extern int64_t slot_set(uint32_t, uint32_t, uint32_t);
            extern int64_t sto_validate(uint32_t, uint32_t);
            extern int64_t hook_account(uint32_t, uint32_t);
            extern int64_t slot_size(uint32_t);
            #define sfBalance ((6U << 16U) + 2U)
            #define sfFlags ((2U << 16U) + 2U)
            #define DOESNT_EXIST -5
            #define TOO_SMALL -4
            #define TOO_BIG -3
            #define OUT_OF_BOUNDS -1
            #define INVALID_ARGUMENT -7
            #define KEYLET_ACCOUNT 3
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                // bounds check
                ASSERT(slot(1, 1000000, 0) == OUT_OF_BOUNDS);
                ASSERT(slot(1000000, 1024, 0) == OUT_OF_BOUNDS);

                // this function can return data as an int64_t,
                // but requires 0,0 as arguments
                ASSERT(slot(0,1, 0) == INVALID_ARGUMENT);
               
                // slot 0 hasn't been set yet so
                ASSERT(slot(0,0,0) == DOESNT_EXIST);

                // grab the hook account
                uint8_t acc[20];
                ASSERT(20 == hook_account(SBUF(acc)));

                // turn it into account root keylet
                uint8_t kl[34];
                ASSERT(34 == util_keylet(SBUF(kl), KEYLET_ACCOUNT, SBUF(acc), 0,0,0,0));

                // slot the account root into a new slot
                int64_t slot_no = 0;
                ASSERT((slot_no = slot_set(SBUF(kl), 0)) > 0);

                int64_t size = 0;
                ASSERT((size = slot_size(slot_no)) > 0);

                // the slotted item is too large for return as int64
                ASSERT(slot(0,0,slot_no) == TOO_BIG);

                // big buffer, large enough to hold the account_root
                uint8_t buf[1024];
                
                // the slot call should return the bytes written which should exactly
                // match the size of the slotted object
                ASSERT(slot(SBUF(buf), slot_no) == size);

                // do a quick sanity check on the object using sto api
                ASSERT(sto_validate(buf, size) == 1);
                
                // grab a field
                ASSERT(sto_subfield(buf, size, sfBalance) > 0);

                // subslot a subfield we can return as an int64_t
                ASSERT(slot_subfield(slot_no, sfBalance, 200) == 200);

                // retrieve the slotted object as an int64_t
                ASSERT(slot(0,0,200) > 0);

                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test slot"), fee(XRP(1)));
    }

    void
    test_slot_clear()
    {
        testcase("Test slot_clear");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot_clear(uint32_t slot_no);
            extern int64_t otxn_slot (uint32_t slot_no);
            extern int64_t slot_size(uint32_t);
            #define sfBalance ((6U << 16U) + 2U)
            #define sfFlags ((2U << 16U) + 2U)
            #define DOESNT_EXIST -5
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                ASSERT(otxn_slot(1) == 1);

                ASSERT(slot_size(1) > 0);

                ASSERT(slot_clear(1) == 1);

                ASSERT(slot_size(1) == DOESNT_EXIST);
                
                ASSERT(slot_clear(1) == DOESNT_EXIST);
                ASSERT(slot_clear(10) == DOESNT_EXIST);

                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot_clear"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test slot_clear"), fee(XRP(1)));
    }

    void
    test_slot_count()
    {
        testcase("Test slot_count");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t otxn_slot (uint32_t slot_no);
            extern int64_t slot_size(uint32_t);
            extern int64_t slot_count(uint32_t);
            extern int64_t slot_subfield (
                uint32_t parent_slot,
                uint32_t field_id,
                uint32_t new_slot);
            #define NOT_AN_ARRAY -22
            #define DOESNT_EXIST -5
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            #define sfMemos ((15U << 16U) + 9U)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                ASSERT(otxn_slot(1) == 1);

                ASSERT(slot_size(1) > 0);

                ASSERT(slot_count(1) == NOT_AN_ARRAY);

                ASSERT(slot_count(0) == DOESNT_EXIST);

                ASSERT(slot_subfield(1, sfMemos, 1) == 1);

                ASSERT(slot_size(1) > 0);
    
                ASSERT(slot_count(1) == 1);

                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot_count"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test slot_count"), fee(XRP(1)));
    }

    void
    test_slot_float()
    {
        testcase("Test slot_float");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t otxn_slot (uint32_t slot_no);
            extern int64_t slot_size(uint32_t);
            extern int64_t slot_count(uint32_t);
            extern int64_t slot_float(uint32_t);
            extern int64_t float_int (
                int64_t float1,
                uint32_t decimal_places,
                uint32_t absolute
            );
            extern int64_t slot_subfield (
                uint32_t parent_slot,
                uint32_t field_id,
                uint32_t new_slot);
            #define NOT_AN_ARRAY -22
            #define DOESNT_EXIST -5
            #define NOT_AN_AMOUNT -32
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            #define sfFee ((6U << 16U) + 8U)  
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                ASSERT(otxn_slot(1) == 1);

                ASSERT(slot_size(1) > 0);

                ASSERT(slot_subfield(1, sfFee, 2) == 2);

                ASSERT(slot_size(2) > 0);
  
                ASSERT(slot_float(0) == DOESNT_EXIST);
            
                ASSERT(slot_float(1) == NOT_AN_AMOUNT);
 
                int64_t xfl = slot_float(2);
                ASSERT(xfl > 0);

                ASSERT(float_int(xfl, 6, 0) == 1000000LL);                

                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot_float"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test slot_float"), fee(XRP(1)));
    }

    void
    test_slot_set()
    {
        testcase("Test slot_set");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t otxn_id(uint32_t, uint32_t, uint32_t);
            extern int64_t slot_set(uint32_t, uint32_t, uint32_t);
            extern int64_t slot_size(uint32_t);
            #define sfBalance ((6U << 16U) + 2U)
            #define sfFlags ((2U << 16U) + 2U)
            #define DOESNT_EXIST -5
            #define OUT_OF_BOUNDS -1
            #define INVALID_ARGUMENT -7
            #define NO_FREE_SLOTS -6
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)

            // skip keylet
            uint8_t kl_sk[] =                                                                                          
            {                                                                                                          
                0x00U, 0x68U,                                                                                          
                0xB4U,0x97U,0x9AU,0x36U,0xCDU,0xC7U,0xF3U,0xD3U,0xD5U,0xC3U,                                           
                0x1AU,0x4EU,0xAEU,0x2AU,0xC7U,0xD7U,0x20U,0x9DU,0xDAU,0x87U,                                           
                0x75U,0x88U,0xB9U,0xAFU,0xC6U,0x67U,0x99U,0x69U,0x2AU,0xB0U,                                           
                0xD6U,0x6BU                                                                                            
            };  

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                // bounds check
                ASSERT(slot_set(1, 1000000, 0) == OUT_OF_BOUNDS);
                ASSERT(slot_set(1000000, 1024, 0) == OUT_OF_BOUNDS);
    
                // read len is only allowed to be 32 (txn id) or 34 (keylet)
                uint8_t kl_zero[34];
                ASSERT(slot_set((uint32_t)kl_zero, 0, 0) == INVALID_ARGUMENT);
                ASSERT(slot_set((uint32_t)kl_zero, 31, 0) == INVALID_ARGUMENT);
                ASSERT(slot_set((uint32_t)kl_zero, 33, 0) == INVALID_ARGUMENT);
                ASSERT(slot_set((uint32_t)kl_zero, 35, 0) == INVALID_ARGUMENT);
                ASSERT(slot_set((uint32_t)kl_zero, 34, 256) == INVALID_ARGUMENT);
                
                // request an invalid keylet
                ASSERT(slot_set(SBUF(kl_zero), 0) == DOESNT_EXIST);
                kl_zero[0] = 1;
                ASSERT(slot_set(SBUF(kl_zero), 0) == DOESNT_EXIST);
                
                ASSERT(slot_size(1) == DOESNT_EXIST);
                // request a valid keylet
                ASSERT(slot_set(SBUF(kl_sk), 0) > 0);
                ASSERT(slot_size(1) > 0);

                // fill up all slots
                for (uint32_t i = 1; GUARD(257), i < 255; ++i)
                    ASSERT(slot_set(SBUF(kl_sk), 0) > 0);
                    
                // request a final slot that should fail
                ASSERT(slot_set(SBUF(kl_sk), 0));

                // overwrite an existing slot, should work
                ASSERT(slot_set(SBUF(kl_sk), 10) == 10);

                // check all the slots contain an object, the same object
                uint32_t s = slot_size(1);
                
                for (uint32_t i = 2; GUARD(257), i < 256; ++i)
                    ASSERT(s == slot_size(i));

                // slot a txn

                uint8_t txn[32];
                ASSERT(otxn_id(SBUF(txn), 0) == 32);
                ASSERT(slot_set(SBUF(txn), 1) == 1);

                uint32_t s2 = slot_size(1);
               
                // ensure it's not the same object that was there before 
                ASSERT(s != s2 && s2 > 0);

                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot_set"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test slot_set"), fee(XRP(1)));
    }

    void
    test_slot_size()
    {
        testcase("Test slot_size");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        Account const alice{"alice"};
        Account const bob{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot_set(uint32_t, uint32_t, uint32_t);
            extern int64_t slot_size(uint32_t);
            extern int64_t slot (uint32_t write_ptr, uint32_t write_len, uint32_t slot_no);
            extern int64_t sto_validate(uint32_t, uint32_t);
            #define sfBalance ((6U << 16U) + 2U)
            #define sfFlags ((2U << 16U) + 2U)
            #define DOESNT_EXIST -5
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define SBUF(x) (uint32_t)(x), sizeof(x)

            // skip keylet
            uint8_t kl_sk[] =                                                                                          
            {                                                                                                          
                0x00U, 0x68U,                                                                                          
                0xB4U,0x97U,0x9AU,0x36U,0xCDU,0xC7U,0xF3U,0xD3U,0xD5U,0xC3U,                                           
                0x1AU,0x4EU,0xAEU,0x2AU,0xC7U,0xD7U,0x20U,0x9DU,0xDAU,0x87U,                                           
                0x75U,0x88U,0xB9U,0xAFU,0xC6U,0x67U,0x99U,0x69U,0x2AU,0xB0U,                                           
                0xD6U,0x6BU                                                                                            
            };  

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                
                ASSERT(slot_size(1) == DOESNT_EXIST);

                // request a valid keylet, twice
                ASSERT(slot_set(SBUF(kl_sk), 1) == 1);
                ASSERT(slot_set(SBUF(kl_sk), 255) == 255);
                
                // check the sizes are equal
                ASSERT(slot_size(1) == slot_size(255));

                // check the sizes are > 0
                int64_t s = slot_size(1);
                ASSERT(s > 0);

                // pull the object out into a buffer, check the number of bytes written is correct
                uint8_t buf[4096];
                ASSERT(slot(SBUF(buf), 1) == s);

                // check the object is valid
                ASSERT(sto_validate(buf, s) == 1);
            
                // done!
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot_size"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test slot_size"), fee(XRP(1)));
    }

    void
    test_slot_subarray()
    {
        testcase("Test slot_subarray");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot_size(uint32_t);
            extern int64_t slot (uint32_t write_ptr, uint32_t write_len, uint32_t slot_no);
            extern int64_t slot_subarray(uint32_t, uint32_t, uint32_t);
            extern int64_t otxn_slot(uint32_t);
            extern int64_t slot_subfield(uint32_t, uint32_t, uint32_t);
            extern int64_t slot_count(uint32_t);
            extern int64_t slot_set(uint32_t, uint32_t, uint32_t);
            #define sfMemos ((15U << 16U) + 9U)
            #define sfMemoData ((7U << 16U) + 13U)
                        
            #define DOESNT_EXIST -5
            #define NO_FREE_SLOTS -6
            #define NOT_AN_ARRAY -22
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
           
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            // skip keylet
            uint8_t kl_sk[] =                                                                                          
            {                                                                                                          
                0x00U, 0x68U,                                                                                          
                0xB4U,0x97U,0x9AU,0x36U,0xCDU,0xC7U,0xF3U,0xD3U,0xD5U,0xC3U,                                           
                0x1AU,0x4EU,0xAEU,0x2AU,0xC7U,0xD7U,0x20U,0x9DU,0xDAU,0x87U,                                           
                0x75U,0x88U,0xB9U,0xAFU,0xC6U,0x67U,0x99U,0x69U,0x2AU,0xB0U,                                           
                0xD6U,0x6BU                                                                                            
            };  

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
               
                ASSERT(slot_subarray(1, 1, 1) == DOESNT_EXIST);
 
                // request a valid keylet that doesn't contain an array
                ASSERT(slot_set(SBUF(kl_sk), 1) == 1);

                ASSERT(slot_size(1) > 0);
    
                ASSERT(slot_subarray(1,1,1) == NOT_AN_ARRAY);

                // now request an object that contains an array (this txn)
                ASSERT(otxn_slot(2) == 2);
    
                // slot the array
                ASSERT(slot_subfield(2, sfMemos, 3) == 3);

                // it should contain 9 entries
                ASSERT(slot_count(3) == 9);

                // now index into the array
                ASSERT(slot_subarray(3, 0, 0) > 0);

                // take element at index 5 and place it in slot 100
                ASSERT(slot_subarray(3, 5, 100) == 100);
                
                // override it and replace with element 6
                ASSERT(slot_subarray(3, 6, 100) == 100);
            
                // check the value is correct
                ASSERT(slot_subfield(100, sfMemoData, 100) == 100);
                
                uint8_t buf[16];

                ASSERT(6 == slot(SBUF(buf), 100));
               
                ASSERT(
                    buf[0] == 0x05U &&
                    buf[1] == 0xC0U && buf[2] == 0x01U && buf[3] == 0xCAU && buf[4] == 0xFEU && buf[5] == 0x06U);

                // override it and replace with element 0
                ASSERT(slot_subarray(3, 0, 100) == 100);
            
                // check the value is correct
                ASSERT(slot_subfield(100, sfMemoData, 100) == 100);
                
                ASSERT(slot(SBUF(buf), 100) == 6);

                ASSERT(
                    buf[0] == 0x05U &&
                    buf[1] == 0xC0U && buf[2] == 0x01U && buf[3] == 0xCAU && buf[4] == 0xFEU && buf[5] == 0x00U);
                
                // test slot exhaustion
                for (int i = 0; GUARD(255), i < 250; ++i)
                    ASSERT(slot_subarray(3, 0, 0) > 0);
                    
                ASSERT(slot_subarray(3, 0, 0) == NO_FREE_SLOTS);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot_subarray"),
            HSFEE);
        env.close();

        // generate an array of memos to attach
        Json::Value jv;
        jv[jss::Account] = bob.human();
        jv[jss::TransactionType] = jss::Payment;
        jv[jss::Flags] = 0;
        jv[jss::Amount] = "1";
        jv[jss::Memos] = Json::Value{Json::arrayValue};
        jv[jss::Destination] = alice.human();
        Json::Value iv;
        for (uint32_t i = 0; i < 8; ++i)
        {
            std::string v = "C001CAFE00";
            v.data()[9] = '0' + i;
            iv[jss::MemoData] = v.c_str();
            iv[jss::MemoFormat] = "";
            iv[jss::MemoType] = "";
            jv[jss::Memos][i][jss::Memo] = iv;
        }

        // invoke the hook
        env(jv, M("test slot_subarray"), fee(XRP(1)));
    }

    void
    test_slot_subfield()
    {
        testcase("Test slot_subfield");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot_size(uint32_t);
            extern int64_t slot (uint32_t write_ptr, uint32_t write_len, uint32_t slot_no);
            extern int64_t otxn_slot(uint32_t);
            extern int64_t slot_subfield(uint32_t, uint32_t, uint32_t);
            extern int64_t slot_count(uint32_t);
            extern int64_t slot_set(uint32_t, uint32_t, uint32_t);
            #define sfMemos ((15U << 16U) + 9U)
            #define sfMemoData ((7U << 16U) + 13U)
            #define sfLastLedgerSequence ((2U << 16U) + 0x1BU)
            #define sfHashes ((19U << 16U) + 2U)
            #define NOT_AN_OBJECT -23
            #define DOESNT_EXIST -5
            #define NO_FREE_SLOTS -6
            #define INVALID_FIELD -17
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
           
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            // skip keylet
            uint8_t kl_sk[] =                                                                                          
            {                                                                                                          
                0x00U, 0x68U,                                                                                          
                0xB4U,0x97U,0x9AU,0x36U,0xCDU,0xC7U,0xF3U,0xD3U,0xD5U,0xC3U,                                           
                0x1AU,0x4EU,0xAEU,0x2AU,0xC7U,0xD7U,0x20U,0x9DU,0xDAU,0x87U,                                           
                0x75U,0x88U,0xB9U,0xAFU,0xC6U,0x67U,0x99U,0x69U,0x2AU,0xB0U,                                           
                0xD6U,0x6BU                                                                                            
            };  

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
               
                ASSERT(slot_subfield(1, 1, 1) == DOESNT_EXIST);
 
                ASSERT(slot_set(SBUF(kl_sk), 1) == 1);

                ASSERT(slot_size(1) > 0);
   
                ASSERT(slot_subfield(1, sfLastLedgerSequence, 0) == 2);

                ASSERT(slot_size(2) >0);
                ASSERT(slot_size(1) > slot_size(2));

                ASSERT(slot_subfield(1, sfHashes, 0) == 3);

                ASSERT(slot_size(3) > 0);
                ASSERT(slot_size(1) > slot_size(3));
                
                // request a field that is invalid
                ASSERT(slot_subfield(1, 0xFFFFFFFFUL, 0) == INVALID_FIELD);

                // request a field that isn't present
                ASSERT(slot_subfield(1, sfMemos, 0) == DOESNT_EXIST);

                // request a subfield from something that's not an object
                ASSERT(slot_subfield(3, sfMemoData, 0) == NOT_AN_OBJECT);

                // overwrite an existing slot
                ASSERT(slot_subfield(1, sfLastLedgerSequence, 3) == 3);
                ASSERT(slot_size(2) == slot_size(3));

                // test slot exhaustion
                for (int i = 0; GUARD(255), i < 252; ++i)
                    ASSERT(slot_subfield(1, sfLastLedgerSequence, 0) > 0);
                    
                ASSERT(slot_subfield(1, sfLastLedgerSequence, 0) == NO_FREE_SLOTS);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot_subfield"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test slot_subfield"), fee(XRP(1)));
    }

    void
    test_slot_type()
    {
        testcase("Test slot_type");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        // set up a trustline which we can retrieve later
        env(trust(alice, bob["USD"](600)));        
        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t slot_size(uint32_t);
            extern int64_t slot (uint32_t write_ptr, uint32_t write_len, uint32_t slot_no);
            extern int64_t otxn_slot(uint32_t);
            extern int64_t slot_subfield(uint32_t, uint32_t, uint32_t);
            extern int64_t slot_count(uint32_t);
            extern int64_t slot_type(uint32_t, uint32_t);
            extern int64_t slot_set(uint32_t, uint32_t, uint32_t);
            extern int64_t otxn_field (
                uint32_t write_ptr,
                uint32_t write_len,
                uint32_t field_id
            );
            extern int64_t util_keylet (
                uint32_t write_ptr,
                uint32_t write_len,
                uint32_t keylet_type,
                uint32_t a,
                uint32_t b,
                uint32_t c,
                uint32_t d,
                uint32_t e,
                uint32_t f
            );
            extern int64_t hook_account(uint32_t, uint32_t);
            #define sfMemos ((15U << 16U) + 9U)
            #define sfMemoData ((7U << 16U) + 13U)
            #define sfLastLedgerSequence ((2U << 16U) + 0x1BU)
            #define sfHashes ((19U << 16U) + 2U)
            #define NOT_AN_OBJECT -23
            #define DOESNT_EXIST -5
            #define NO_FREE_SLOTS -6
            #define INVALID_FIELD -17
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
           
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            // skip keylet
            uint8_t kl_sk[] =                                                                                          
            {                                                                                                          
                0x00U, 0x68U,                                                                                          
                0xB4U,0x97U,0x9AU,0x36U,0xCDU,0xC7U,0xF3U,0xD3U,0xD5U,0xC3U,                                           
                0x1AU,0x4EU,0xAEU,0x2AU,0xC7U,0xD7U,0x20U,0x9DU,0xDAU,0x87U,                                           
                0x75U,0x88U,0xB9U,0xAFU,0xC6U,0x67U,0x99U,0x69U,0x2AU,0xB0U,                                           
                0xD6U,0x6BU                                                                                            
            };  
            #define sfLedgerEntry ((10002U << 16U) + 257U)
            #define sfTransaction ((10001U << 16U) + 257U)
            #define sfAmount ((6U << 16U) + 1U)
            #define sfHighLimit ((6U << 16U) + 7U)
            #define sfAccount ((8U << 16U) + 1U)
            #define NOT_AN_AMOUNT -32
            #define KEYLET_LINE 9
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
               
                ASSERT(slot_type(1, 0) == DOESNT_EXIST);
 
                ASSERT(slot_set(SBUF(kl_sk), 1) == 1);

                ASSERT(slot_size(1) > 0);
 
                ASSERT(slot_type(1, 0) == sfLedgerEntry);   

                ASSERT(slot_subfield(1, sfLastLedgerSequence, 0) == 2);

                ASSERT(slot_size(2) >0);

                ASSERT(slot_size(1) > slot_size(2));

                ASSERT(slot_type(2, 0) == sfLastLedgerSequence);

                ASSERT(otxn_slot(3) == 3);

                ASSERT(slot_type(3, 0) == sfTransaction);

                ASSERT(slot_subfield(3, sfAmount, 4) == 4);

                // this will determine if the amount is native by returning 1 if it is
                ASSERT(slot_type(4, 1) == 1);

                ASSERT(slot_type(3, 1) == NOT_AN_AMOUNT);

                // there's a trustline between alice and bob
                // we can find alice and bob's addresses from otxn
                uint8_t addra[20];
                uint8_t addrb[20];
                ASSERT(hook_account(SBUF(addra)) == 20);
                ASSERT(otxn_field(SBUF(addrb), sfAccount) == 20);

                // build the keylet for the tl
                uint8_t kl_tr[34];
                ASSERT(util_keylet(SBUF(kl_tr), KEYLET_LINE, SBUF(addra), SBUF(addrb), "USD", 3) == 34);

                // slot the ripplestate object
                ASSERT(slot_set(SBUF(kl_tr), 5) == 5);

                // subfield into the high limit
                ASSERT(slot_subfield(5, sfHighLimit, 6) == 6);

                // this is a non-native balance so we should get 0 back when testing the amount type
                ASSERT(slot_type(6, 1) == 0);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set slot_subfield"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test slot_type"), fee(XRP(1)));
    }

    void
    test_state()
    {
        testcase("Test state");
        using namespace jtx;


        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t state (
                    uint32_t write_ptr,
                    uint32_t write_len,
                    uint32_t kread_ptr,
                    uint32_t kread_len  
                );
                extern int64_t state_set(uint32_t,uint32_t,uint32_t, uint32_t);
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);

                #define TOO_BIG (-3)
                #define TOO_SMALL (-4)
                #define OUT_OF_BOUNDS (-1)
                #define SBUF(x) (uint32_t)(x), sizeof(x)
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // set a state object
                    ASSERT(state_set(SBUF("content"), SBUF("key")) == sizeof("content"));
                    ASSERT(state_set(SBUF("content2"), SBUF("key2")) == sizeof("content2"));

                    // Test out of bounds check
                    ASSERT(state(1000000, 32, 0, 32) == OUT_OF_BOUNDS);
                    ASSERT(state(0, 1000000, 0, 32) == OUT_OF_BOUNDS);
                    ASSERT(state(0, 32, 1000000, 32) == OUT_OF_BOUNDS);
                    ASSERT(state(0, 32, 0, 1000000) == TOO_BIG);
                    ASSERT(state(0,0,0,0) == TOO_SMALL);
                    ASSERT(state(0,0,0,33) == TOO_BIG);

                    // read state back
                    uint8_t buf1[32];
                    uint8_t buf2[32];

                    int64_t bytes1 = state(SBUF(buf1), SBUF("key"));
                    ASSERT(bytes1 == sizeof("content"));

                    int64_t bytes2 = state(SBUF(buf2), SBUF("key2"));
                    ASSERT(bytes2 == sizeof("content2"));
                    
                    for (int i = 32; GUARD(32), i < bytes1; ++i)
                        ASSERT(buf1[i] == *((uint8_t*)"content" + i));
                    
                    for (int i = 32; GUARD(32), i < bytes2; ++i)
                        ASSERT(buf2[i] == *((uint8_t*)"content2" + i));

                    // RH TODO:
                    // - read small state back as int64
                    
                    return accept(0,0,0);
                }
            )[test.hook]"];

            // install the hook on alice
            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set state"),
                HSFEE);
            env.close();

            // invoke the hook
            env(pay(bob, alice, XRP(1)), M("test state"), fee(XRP(1)));
        }

        // override hook with a second version that just reads those state objects
        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t state (
                    uint32_t write_ptr,
                    uint32_t write_len,
                    uint32_t kread_ptr,
                    uint32_t kread_len  
                );
                extern int64_t state_set(uint32_t,uint32_t,uint32_t, uint32_t);
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);

                #define SBUF(x) (uint32_t)(x), sizeof(x)
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // read state back
                    uint8_t buf1[32];
                    uint8_t buf2[32];

                    int64_t bytes1 = state(SBUF(buf1), SBUF("key"));
                    ASSERT(bytes1 == sizeof("content"));

                    int64_t bytes2 = state(SBUF(buf2), SBUF("key2"));
                    ASSERT(bytes2 == sizeof("content2"));
                    
                    for (int i = 32; GUARD(32), i < bytes1; ++i)
                        ASSERT(buf1[i] == *((uint8_t*)"content" + i));
                    
                    for (int i = 32; GUARD(32), i < bytes2; ++i)
                        ASSERT(buf2[i] == *((uint8_t*)"content2" + i));
                    
                    return accept(0,0,0);
                }
            )[test.hook]"];

            // install the hook on alice
            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set state 2"),
                HSFEE);
            env.close();

            // invoke the hook
            env(pay(bob, alice, XRP(1)), M("test state 2"), fee(XRP(1)));
        }
    }

    void
    test_state_foreign()
    {
        testcase("Test state_foreign");
        using namespace jtx;


        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t state (
                    uint32_t write_ptr,
                    uint32_t write_len,
                    uint32_t kread_ptr,
                    uint32_t kread_len  
                );
                extern int64_t state_foreign_set(
                    uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
                extern int64_t hook_account(uint32_t, uint32_t);
                extern int64_t state_set(uint32_t,uint32_t,uint32_t, uint32_t);
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);

                #define SBUF(x) (uint32_t)(x), sizeof(x)
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);

                    // set a state object
                    ASSERT(state_set(SBUF("content"), SBUF("key")) == sizeof("content"));

                    // put the second state object on a different ns

                    uint8_t ns[32];
                    ns[9] = 0xABU;

                    uint8_t acc[20];
                    ASSERT(hook_account(SBUF(acc)) == 20);
                    ASSERT(state_foreign_set(SBUF("content2"), SBUF("key2"), SBUF(ns), SBUF(acc)) == sizeof("content2"));

                    return accept(0,0,0);
                }
            )[test.hook]"];

            // install the hook on alice
            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set state_foreign"),
                HSFEE);
            env.close();

            // invoke the hook
            env(pay(bob, alice, XRP(1)), M("test state_foreign"), fee(XRP(1)));
        }

        // set a second hook on bob that will read the state objects from alice
        {
            TestHook hook = wasm[R"[test.hook](
                #include <stdint.h>
                extern int32_t _g       (uint32_t id, uint32_t maxiter);
                #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
                extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
                extern int64_t state (
                    uint32_t write_ptr,
                    uint32_t write_len,
                    uint32_t kread_ptr,
                    uint32_t kread_len  
                );
                extern int64_t state_foreign (
                    uint32_t, uint32_t, uint32_t, uint32_t,
                    uint32_t, uint32_t, uint32_t, uint32_t
                );
                extern int64_t hook_account(uint32_t write_ptr, uint32_t write_len);
                extern int64_t state_set(uint32_t,uint32_t,uint32_t, uint32_t);
                extern int64_t otxn_field(uint32_t, uint32_t, uint32_t);
                #define ASSERT(x)\
                    if (!(x))\
                        rollback((uint32_t)#x, sizeof(#x), __LINE__);

                #define TOO_BIG (-3)
                #define TOO_SMALL (-4)
                #define OUT_OF_BOUNDS (-1)
                #define DOESNT_EXIST (-5)
                #define INVALID_ARGUMENT (-7)
                #define SBUF(x) (uint32_t)(x), sizeof(x)
                #define sfAccount ((8U << 16U) + 1U)
                int64_t hook(uint32_t reserved )
                {
                    _g(1,1);


                    // Test out of bounds check
                    int64_t y;
                    ASSERT((y=state_foreign(1111111, 32, 1, 32, 1, 32, 1, 20)) == OUT_OF_BOUNDS);
                    ASSERT((y=state_foreign(1, 1111111, 1, 32, 1, 32, 1,  20)) == OUT_OF_BOUNDS);
                    ASSERT((y=state_foreign(1, 32, 1111111, 32, 1, 32, 1, 20)) == OUT_OF_BOUNDS);
                    ASSERT((y=state_foreign(1, 32, 1, 1111111, 1, 32, 1,  20)) == TOO_BIG);
                    ASSERT((y=state_foreign(1, 32, 1, 32, 1111111, 32, 1, 20)) == OUT_OF_BOUNDS);
                    ASSERT((y=state_foreign(1, 32, 1, 32, 1, 1111111, 1,  20)) == INVALID_ARGUMENT);
                    ASSERT((y=state_foreign(1, 32, 1, 32, 1, 32, 1111111, 20)) == OUT_OF_BOUNDS);
                    ASSERT((y=state_foreign(1, 32, 1, 32, 1, 32, 1, 1111111)) == INVALID_ARGUMENT);


                    // alice's address is the sender
                    uint8_t acc[20];
                    ASSERT(otxn_field(SBUF(acc), sfAccount) == 20);

                    // read state back
                    uint8_t buf1[32];
                    uint8_t buf2[32];

                    // the namespace of the first obj is all zeros
                    uint8_t ns[32];                    
                    int64_t bytes1 = state_foreign(SBUF(buf1), SBUF("key"), SBUF(ns), SBUF(acc));
                    ASSERT(bytes1 == sizeof("content"));

                    // the namespace of the second obj is all zeros except position 9 which is 0xAB
                    // ensure the namespacing is working by requesting against the wrong namespace first
                    int64_t bytes2 = state_foreign(SBUF(buf2), SBUF("key2"), SBUF(ns), SBUF(acc));
                    ASSERT(bytes2 == DOESNT_EXIST);
                    ns[9] = 0xABU;
                    bytes2 = state_foreign(SBUF(buf2), SBUF("key2"), SBUF(ns), SBUF(acc));
                    ASSERT(bytes2 == sizeof("content2"));
                    
                    for (int i = 32; GUARD(32), i < bytes1; ++i)
                        ASSERT(buf1[i] == *((uint8_t*)"content" + i));
                    
                    for (int i = 32; GUARD(32), i < bytes2; ++i)
                        ASSERT(buf2[i] == *((uint8_t*)"content2" + i));
                    
                    return accept(0,0,0);
                }
            )[test.hook]"];

            // install the hook on bob 
            env(ripple::test::jtx::hook(bob, {{hso(hook, overrideFlag)}}, 0),
                M("set state_foreign 2"),
                HSFEE);
            env.close();

            // invoke the hook
            
            env(pay(alice, bob, XRP(1)), M("test state_foreign 2"), fee(XRP(1)));
        }
    }

    void
    test_state_foreign_set()
    {
        testcase("Test state_foreign_set");
        using namespace jtx;


        Env env{*this, supported_amendments()};

        auto const david = Account("david"); // grantee generic
        auto const cho = Account{"cho"};     // invoker
        auto const bob = Account{"bob"};     // grantee specific
        auto const alice = Account{"alice"}; // grantor
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);
        env.fund(XRP(10000), cho);
        env.fund(XRP(10000), david);

        TestHook grantee_wasm = wasm[R"[test.hook](
            #include <stdint.h>
            #define sfInvoiceID ((5U << 16U) + 17U)
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t state_foreign (
                uint32_t, uint32_t, uint32_t, uint32_t,
                uint32_t, uint32_t, uint32_t, uint32_t
            );
            extern int64_t state_foreign_set (
                uint32_t, uint32_t, uint32_t, uint32_t,
                uint32_t, uint32_t, uint32_t, uint32_t
            );
            extern int64_t otxn_id(uint32_t, uint32_t, uint32_t);
            extern int64_t otxn_field(uint32_t, uint32_t, uint32_t);
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);

            #define TOO_BIG (-3)
            #define TOO_SMALL (-4)
            #define OUT_OF_BOUNDS (-1)
            #define DOESNT_EXIST (-5)
            #define INVALID_ARGUMENT (-7)
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                // bounds tests
                int64_t y;
                ASSERT((y=state_foreign_set(1111111, 32, 1, 32, 1, 32, 1, 20)) == OUT_OF_BOUNDS);
                ASSERT((y=state_foreign_set(1, 1111111, 1, 32, 1, 32, 1,  20)) == OUT_OF_BOUNDS);
                ASSERT((y=state_foreign_set(1, 32, 1111111, 32, 1, 32, 1, 20)) == OUT_OF_BOUNDS);
                ASSERT((y=state_foreign_set(1, 32, 1, 1111111, 1, 32, 1,  20)) == TOO_BIG);
                ASSERT((y=state_foreign_set(1, 32, 1, 32, 1111111, 32, 1, 20)) == OUT_OF_BOUNDS);
                ASSERT((y=state_foreign_set(1, 32, 1, 32, 1, 1111111, 1,  20)) == INVALID_ARGUMENT);
                ASSERT((y=state_foreign_set(1, 32, 1, 32, 1, 32, 1111111, 20)) == OUT_OF_BOUNDS);
                ASSERT((y=state_foreign_set(1, 32, 1, 32, 1, 32, 1, 1111111)) == INVALID_ARGUMENT);

                // get this transaction id
                uint8_t txn[32];
                ASSERT(otxn_id(SBUF(txn), 0) == 32);

                // get the invoice id, which contains the grantor account
                uint8_t grantor[32];
                ASSERT(otxn_field(SBUF(grantor), sfInvoiceID) == 32);

                // set the current txn id on the grantor's state under key 1, namespace 0
                uint8_t one[32]; one[31] = 1U;
                uint8_t zero[32];
                ASSERT(state_foreign_set(SBUF(txn), SBUF(one), SBUF(zero), grantor + 12, 20) == 32);
               
                return accept(0,0,0);
            }
        )[test.hook]"];

        HASH_WASM(grantee);

        // this is the grantor 
        TestHook grantor_wasm = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t otxn_field (uint32_t, uint32_t, uint32_t);
            extern int64_t hook_account(uint32_t write_ptr, uint32_t write_len);
            extern int64_t state_foreign_set (
                uint32_t, uint32_t, uint32_t, uint32_t,
                uint32_t, uint32_t, uint32_t, uint32_t
            );
            #define DOESNT_EXIST (-5)
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);

            #define BUFFER_EQUAL_20(buf1, buf2)\
                (\
                    *(((uint64_t*)(buf1)) + 0) == *(((uint64_t*)(buf2)) + 0) &&\
                    *(((uint64_t*)(buf1)) + 1) == *(((uint64_t*)(buf2)) + 1) &&\
                    *(((uint32_t*)(buf1)) + 4) == *(((uint32_t*)(buf2)) + 4))
            #define SBUF(x) (uint32_t)(x), sizeof(x)

            #define sfAccount ((8U << 16U) + 1U)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t otxnacc[20];
                ASSERT(otxn_field(SBUF(otxnacc), sfAccount) == 20);

                uint8_t hookacc[20];
                ASSERT(hook_account(SBUF(hookacc)) == 20);

                if (BUFFER_EQUAL_20(otxnacc, hookacc))
                {
                    // outgoin txn, delete the state
                    uint8_t one[32]; one[31] = 1U;
                    uint8_t zero[32];
                    // we can use state_foreign_set to do the deletion, a concise way to test this functionality too
                    int64_t y = state_foreign_set(0,0, SBUF(one), SBUF(zero), SBUF(hookacc));
                    ASSERT(y == 0 || y == DOESNT_EXIST);
                }
                

                return accept(0,0,0);
            }
        )[test.hook]"];
            
        HASH_WASM(grantor);

        // install the grantor hook on alice
        {
            Json::Value grants{Json::arrayValue};
            grants[0U][jss::HookGrant] = Json::Value{};
            grants[0U][jss::HookGrant][jss::HookHash] = grantee_hash_str;
            grants[0U][jss::HookGrant][jss::Authorize] = bob.human(); 
            
            Json::Value json = ripple::test::jtx::hook(alice, {{hso(grantor_wasm, overrideFlag)}}, 0);
            json[jss::Hooks][0U][jss::Hook][jss::HookGrants] = grants;


            env(json,
                M("set state_foreign_set"),
                HSFEE);
            env.close();
        }

        
        // install the grantee hook on bob
        {
            
            // invoice ID contains the grantor account
            Json::Value json = ripple::test::jtx::hook(bob, {{hso(grantee_wasm, overrideFlag)}}, 0);
            env(json,
                M("set state_foreign_set 2"),
                HSFEE);
            env.close();
        }

        auto const aliceid = Account("alice").id();
        auto const nsdirkl = keylet::hookStateDir(aliceid, beast::zero);

        std::string const invid = std::string(24, '0') + strHex(alice.id());

        auto const one = 
            ripple::uint256("0000000000000000000000000000000000000000000000000000000000000001");

        // ensure there's no way the state or directory exist before we start
        {
            auto const nsdir = env.le(nsdirkl);
            BEAST_REQUIRE(!nsdir);

            auto const state1 = env.le(ripple::keylet::hookState(aliceid, one, beast::zero));
            BEAST_REQUIRE(!state1);

            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 2);
        }

        // inovke the grantee hook but supply an account to foreign_set (through invoiceid)
        // should definitely fail 
        {
            Json::Value json = pay(cho, bob, XRP(1));
            json[jss::InvoiceID] = "0000000000000000000000000000000000000000000000000000000000000001";
            env(json, fee(XRP(1)), M("test state_foreign_set 6a"), ter(tecHOOK_REJECTED));
        }

        // invoke the grantee hook but supply a valid account for which no grants exist
        {
            Json::Value json = pay(cho, bob, XRP(1));
            json[jss::InvoiceID] = std::string(24, '0') + strHex(david.id());
            env(json, fee(XRP(1)),
                M("test state_foreign_set 6b"),
                ter(tecHOOK_REJECTED));
            {
                auto const nsdir = env.le(nsdirkl);
                BEAST_REQUIRE(!nsdir);

                auto const state1 = env.le(ripple::keylet::hookState(david.id(), one, beast::zero));
                BEAST_REQUIRE(!state1);

                BEAST_EXPECT((*env.le("david"))[sfOwnerCount] == 0);
            }
        }

        // invoke the grantee hook, this will create the state on the grantor 
        {
            Json::Value json = pay(cho, bob, XRP(1));
            json[jss::InvoiceID] = invid;
            env(json, fee(XRP(1)),
                M("test state_foreign_set 6"),
                ter(tesSUCCESS));
        }
       
        // check state
        {
            auto const nsdir = env.le(nsdirkl);
            BEAST_REQUIRE(!!nsdir);

            auto const state1 = env.le(ripple::keylet::hookState(aliceid, one, beast::zero));
            BEAST_REQUIRE(!!state1);

            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 3);
            
            BEAST_EXPECT(state1->getFieldH256(sfHookStateKey) == one);
        
            auto const data1 = state1->getFieldVL(sfHookStateData);
            BEAST_EXPECT(data1.size() == 32);
            BEAST_EXPECT(uint256::fromVoid(data1.data()) == env.txid());
        }
        

        // invoke the grantor hook, this will delete the state
        env(pay(alice, cho, XRP(1)), M("test state_foreign_set 4"), fee(XRP(1)));

        // check state was removed
        {
            auto const nsdir = env.le(nsdirkl);
            BEAST_REQUIRE(!nsdir);

            auto const state1 = env.le(ripple::keylet::hookState(aliceid, one, beast::zero));
            BEAST_REQUIRE(!state1);

            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 2);
        }

        // install grantee hook on david
        {
            
            // invoice ID contains the grantor account
            Json::Value json = ripple::test::jtx::hook(david, {{hso(grantee_wasm, overrideFlag)}}, 0);
            env(json,
                M("set state_foreign_set 5"),
                HSFEE);
            env.close();
        }

        // invoke daivd, expect failure
        {
            Json::Value json = pay(cho, david, XRP(1));
            json[jss::InvoiceID] = invid;
            env(json, fee(XRP(1)),
                M("test state_foreign_set 6"),
                ter(tecHOOK_REJECTED));
        }
        
        // remove sfAuthorize from alice grants
        {
            Json::Value grants{Json::arrayValue};
            grants[0U][jss::HookGrant] = Json::Value{};
            grants[0U][jss::HookGrant][jss::HookHash] = grantee_hash_str;
            
            Json::Value json = ripple::test::jtx::hook(alice, {{hso(grantor_wasm, overrideFlag)}}, 0);
            json[jss::Hooks][0U][jss::Hook][jss::HookGrants] = grants;

            env(json,
                M("set state_foreign_set 7"),
                HSFEE);
            env.close();
        }

        // invoke david again, expect success
        {
            Json::Value json = pay(cho, david, XRP(1));
            json[jss::InvoiceID] = invid;
            env(json, fee(XRP(1)),
                M("test state_foreign_set 8"),
                 ter(tesSUCCESS));
        }
        
        // check state
        {
            auto const nsdir = env.le(nsdirkl);
            BEAST_REQUIRE(!!nsdir);

            auto const state1 = env.le(ripple::keylet::hookState(aliceid, one, beast::zero));
            BEAST_REQUIRE(!!state1);

            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 3);
            
            BEAST_EXPECT(state1->getFieldH256(sfHookStateKey) == one);
        
            auto const data1 = state1->getFieldVL(sfHookStateData);
            BEAST_EXPECT(data1.size() == 32);
            BEAST_EXPECT(uint256::fromVoid(data1.data()) == env.txid());
        }

        // change alice's namespace
        {
            
            Json::Value json = ripple::test::jtx::hook(alice, {{hso(grantor_wasm, overrideFlag)}}, 0);
            json[jss::Hooks][0U][jss::Hook][jss::HookNamespace] = 
                    "7777777777777777777777777777777777777777777777777777777777777777";
            env(json,
                M("set state_foreign_set 9"),
                HSFEE);
            env.close();
        }


        // invoke david again, expect failure
        {
            Json::Value json = pay(cho, david, XRP(1));
            json[jss::InvoiceID] = invid;
            env(json, fee(XRP(1)),
                M("test state_foreign_set 10"),
                 ter(tecHOOK_REJECTED));
        }
        // RH TODO: check reserve exhaustion
    }

    void
    test_state_set()
    {
        testcase("Test state_set");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        auto const cho = Account{"cho"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);
        env.fund(XRP(10000), cho);

        

        // install a rollback hook on cho
        env(ripple::test::jtx::hook(cho, {{hso(rollback_wasm, overrideFlag)}}, 0),
            M("set state_set rollback"),
            HSFEE);
        env.close();

        auto const aliceid = Account("alice").id();

        auto const nsdirkl = keylet::hookStateDir(aliceid, beast::zero);

        // ensure there's no way the state or directory exist before we start
        {
            auto const nsdir = env.le(nsdirkl);
            BEAST_REQUIRE(!nsdir);

            auto const state1 = env.le(ripple::keylet::hookState(aliceid, beast::zero, beast::zero));
            BEAST_REQUIRE(!state1);

            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 0);
        }

        // first hook will set two state objects with different keys and data on alice
        {
            TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t state_set (
                uint32_t read_ptr,
                uint32_t read_len,
                uint32_t kread_ptr,
                uint32_t kread_len  
            );
            
            uint8_t data2[128] = 
            {
                0x23U,0x13U,0x96U,0x68U,0x78U,0xDCU,0xABU,0xC4U,0x40U,0x26U,
                0x07U,0x2BU,0xA3U,0xD2U,0x0CU,0x69U,0x40U,0xDDU,0xCDU,0xE7U,
                0x38U,0x9BU,0x0BU,0xA9U,0x6CU,0x3CU,0xB3U,0x87U,0x37U,0x02U,
                0x81U,0xE8U,0x2BU,0xDDU,0x5DU,0xBBU,0x40U,0xD9U,0x66U,0x96U,
                0x6FU,0xC1U,0x6BU,0xE8U,0xD4U,0x7CU,0x7BU,0x62U,0x14U,0x4CU,
                0xD1U,0x4BU,0xAAU,0x99U,0x36U,0x75U,0xE9U,0x22U,0xADU,0x0FU,
                0x5FU,0x94U,0x1DU,0x86U,0xEBU,0xA8U,0x13U,0x99U,0xF9U,0x98U,
                0xFFU,0xCAU,0x5BU,0x86U,0x2FU,0xDFU,0x67U,0x8FU,0xE2U,0xE3U,
                0xC3U,0x37U,0xCCU,0x47U,0x0FU,0x33U,0x88U,0xB0U,0x33U,0x3BU,
                0x02U,0x55U,0x67U,0x16U,0xA4U,0xFBU,0x8EU,0x85U,0x6FU,0xD8U,
                0x84U,0x16U,0xA3U,0x54U,0x18U,0x34U,0x06U,0x0EU,0xF6U,0x65U,
                0x34U,0x05U,0x26U,0x7EU,0x05U,0x74U,0xDAU,0x09U,0xBFU,0x55U,
                0x8CU,0x75U,0x92U,0xACU,0x33U,0xFBU,0x01U,0x8DU
            };

            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define TOO_SMALL (-4)
            #define TOO_BIG (-3)
            #define OUT_OF_BOUNDS (-1)
           
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);


                // bounds and buffer size checks
                {
                    // RH NOTE: readptr/len 0/0 = delete entry

                    ASSERT(state_set(0,0,0,0) == TOO_SMALL);
                    ASSERT(state_set(0,0,0,33) == TOO_BIG);
                    ASSERT(state_set(0,0,0,1000000) == TOO_BIG);
                    ASSERT(state_set(0,0,1000000,1) == OUT_OF_BOUNDS);

                    ASSERT(state_set(0,1000000, 0, 32) == OUT_OF_BOUNDS); 
                    ASSERT(state_set(1000000, 0, 0, 32) == OUT_OF_BOUNDS); 
     
                    ASSERT(state_set(0, 257, 0, 32) == TOO_BIG);
                }


                // create state 1
                {
                    uint8_t key[32] =
                    {
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0
                    };

                    uint8_t data[4] = 
                    {
                        0xCAU,0xFEU,0xBAU,0xBEU
                    };


                    ASSERT(state_set(SBUF(data), SBUF(key)) == sizeof(data));
                }

                // create state 2                
                {
                    uint8_t key[3] =
                    {
                        1,2,3
                    };


                    ASSERT(state_set(SBUF(data2), SBUF(key)) == sizeof(data2));
                }


                accept(0,0,0);

            }
            )[test.hook]"];

            // install the hook on alice
            env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
                M("set state_set 1"),
                HSFEE);
            env.close();
            
            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 1);

            // invoke the hook with cho (rollback after alice's hooks have executed)
            env(pay(alice, cho, XRP(1)), M("test state_set 1 rollback"), fee(XRP(1)), ter(tecHOOK_REJECTED));

            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 1);
           
            auto const nsdir = env.le(nsdirkl);
            BEAST_EXPECT(!nsdir);

            auto const state1 = env.le(ripple::keylet::hookState(aliceid, beast::zero, beast::zero));
            BEAST_EXPECT(!state1);

            // invoke the hook from bob to alice, this will work
            env(pay(bob, alice, XRP(1)), M("test state_set 1"), fee(XRP(1)));
            env.close();
        }

        // check that the state object and namespace exists
        {
    
            // owner count should be 1 hook +  2 state objects == 3
            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 3);

            auto const nsdir = env.le(nsdirkl);
            BEAST_REQUIRE(!!nsdir);

            BEAST_EXPECT(nsdir->getFieldV256(sfIndexes).size() == 2);

            auto const state1 = env.le(ripple::keylet::hookState(aliceid, beast::zero, beast::zero));
            BEAST_REQUIRE(!!state1);

            BEAST_EXPECT(state1->getFieldH256(sfHookStateKey) == beast::zero);
        
            auto const data1 = state1->getFieldVL(sfHookStateData);
            BEAST_EXPECT(data1.size() == 4);
            BEAST_EXPECT(data1[0] == 0xCAU && data1[1] == 0xFEU && data1[2] == 0xBAU && data1[3] == 0xBEU);
            
            uint8_t key2[32] = 
            {
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,1,2,3
            };

            auto const state2 = env.le(ripple::keylet::hookState(aliceid, uint256::fromVoid(key2), beast::zero));

            BEAST_REQUIRE(!!state2);
        
            auto const lekey2 = state2->getFieldH256(sfHookStateKey);
            
            BEAST_EXPECT(lekey2 == uint256::fromVoid(key2));
            
            uint8_t data2[128] = 
            {
                0x23U,0x13U,0x96U,0x68U,0x78U,0xDCU,0xABU,0xC4U,0x40U,0x26U,
                0x07U,0x2BU,0xA3U,0xD2U,0x0CU,0x69U,0x40U,0xDDU,0xCDU,0xE7U,
                0x38U,0x9BU,0x0BU,0xA9U,0x6CU,0x3CU,0xB3U,0x87U,0x37U,0x02U,
                0x81U,0xE8U,0x2BU,0xDDU,0x5DU,0xBBU,0x40U,0xD9U,0x66U,0x96U,
                0x6FU,0xC1U,0x6BU,0xE8U,0xD4U,0x7CU,0x7BU,0x62U,0x14U,0x4CU,
                0xD1U,0x4BU,0xAAU,0x99U,0x36U,0x75U,0xE9U,0x22U,0xADU,0x0FU,
                0x5FU,0x94U,0x1DU,0x86U,0xEBU,0xA8U,0x13U,0x99U,0xF9U,0x98U,
                0xFFU,0xCAU,0x5BU,0x86U,0x2FU,0xDFU,0x67U,0x8FU,0xE2U,0xE3U,
                0xC3U,0x37U,0xCCU,0x47U,0x0FU,0x33U,0x88U,0xB0U,0x33U,0x3BU,
                0x02U,0x55U,0x67U,0x16U,0xA4U,0xFBU,0x8EU,0x85U,0x6FU,0xD8U,
                0x84U,0x16U,0xA3U,0x54U,0x18U,0x34U,0x06U,0x0EU,0xF6U,0x65U,
                0x34U,0x05U,0x26U,0x7EU,0x05U,0x74U,0xDAU,0x09U,0xBFU,0x55U,
                0x8CU,0x75U,0x92U,0xACU,0x33U,0xFBU,0x01U,0x8DU
            };

            auto const ledata2 = state2->getFieldVL(sfHookStateData);
            BEAST_REQUIRE(ledata2.size() == sizeof(data2));

            for(uint32_t i = 0; i < sizeof(data2); ++i)
                BEAST_EXPECT(data2[i] == ledata2[i]);

        }

        // make amother hook to override an existing state and delete an existing state
        {
            TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t state_set (
                uint32_t read_ptr,
                uint32_t read_len,
                uint32_t kread_ptr,
                uint32_t kread_len  
            );
            
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
           
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);


                // override state 1
                {

                    uint8_t data[16] = 
                    {
                        1,1,1,1,1,1,1,1,
                        1,1,1,1,1,1,1,2
                    };


                    uint8_t zero = 0;

                    ASSERT(state_set(SBUF(data), &zero, 1) == sizeof(data));
                }

                // delete state 2
                {
                    uint8_t key2[32] = 
                    {
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,
                        0,0,0,0,0,1,2,3
                    };
                    uint8_t zero[1] = {0};
                    ASSERT(state_set(0,0, key2, 32) == 0);
                }

                accept(0,0,0);

            }
            )[test.hook]"];

            TestHook hook2 = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t state (
                uint32_t write_ptr,
                uint32_t write_len,
                uint32_t kread_ptr,
                uint32_t kread_len  
            );
            
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
           
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                // verify updated state
                {

                    uint8_t data[16] = 
                    {
                        1,1,1,1,1,1,1,1,
                        1,1,1,1,1,1,1,2
                    };

                    uint8_t data_read[16];
                    uint8_t zero = 0;

                    ASSERT(state(SBUF(data_read), &zero, 1) == sizeof(data));

                    for (uint32_t i = 0; GUARD(16), i < 16; ++i)
                        ASSERT(data[i] == data_read[i]);
                }

                accept(0,0,0);

            }
            )[test.hook]"];
            // install the hook on alice
            env(ripple::test::jtx::hook(alice, {{{hso(hook, overrideFlag)}, {}, {}, {hso(hook2, 0)}}}, 0),
                M("set state_set 2"),
                HSFEE);
            env.close();

            // two hooks + two state objects = 4
            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 4);
           
            // this hook will be installed on bob, and it will verify the newly updated state
            // is also available on his side. caution must be taken because bob's hooks will execute
            // first if bob's is the otxn. therefore we will flip to a payment from alice to bob here
            TestHook hook3 = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t hook_again(void);
            extern int64_t state_foreign (
                uint32_t write_ptr,
                uint32_t write_len,
                uint32_t kread_ptr,
                uint32_t kread_len,
                uint32_t nread_ptr,
                uint32_t nread_len,
                uint32_t aread_ptr,
                uint32_t aread_len
            );
            extern int64_t otxn_field(uint32_t, uint32_t, uint32_t);
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define sfAccount ((8U << 16U) + 1U) 
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                hook_again();   // we're going to check in weak execution too

                uint8_t alice[20];
                ASSERT(otxn_field(SBUF(alice), sfAccount) == 20);

                // verify updated state
                {

                    uint8_t data[16] = 
                    {
                        1,1,1,1,1,1,1,1,
                        1,1,1,1,1,1,1,2
                    };

                    uint8_t data_read[16];
                    uint8_t zero[32] =
                    {
                        0,0,0,0, 0,0,0,0,
                        0,0,0,0, 0,0,0,0,
                        0,0,0,0, 0,0,0,0,
                        0,0,0,0, 0,0,0,0
                    };

                    ASSERT(state_foreign(SBUF(data_read), SBUF(zero), SBUF(zero), SBUF(alice)) == sizeof(data));

                    for (uint32_t i = 0; GUARD(16), i < 16; ++i)
                        ASSERT(data[i] == data_read[i]);
                }

                accept(0,0,0);

            }
            )[test.hook]"];
            
            // install the hook on bob
            env(ripple::test::jtx::hook(bob, {{hso(hook3, overrideFlag)}}, 0),
                M("set state_set 3"),
                HSFEE);
            env.close();
            
            // invoke the hook with cho (rollback after alice's hooks have executed)
            env(pay(alice, cho, XRP(1)), M("test state_set 3 rollback"), fee(XRP(1)), ter(tecHOOK_REJECTED));
            
            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 4);

            // invoke the hook
            env(pay(alice, bob, XRP(1)), M("test state_set 3"), fee(XRP(1)));
            env.close();

        }
        
        // check that the updates have been made
        {
    
            // two hooks + one state == 3
            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 3);
            BEAST_EXPECT((*env.le("bob"))[sfOwnerCount] == 1);

            auto const nsdir = env.le(nsdirkl);
            BEAST_REQUIRE(!!nsdir);

            BEAST_EXPECT(nsdir->getFieldV256(sfIndexes).size() == 1);

            auto const state1 = env.le(ripple::keylet::hookState(aliceid, beast::zero, beast::zero));
            BEAST_REQUIRE(!!state1);

            BEAST_EXPECT(state1->getFieldH256(sfHookStateKey) == beast::zero);
        
            auto const ledata1 = state1->getFieldVL(sfHookStateData);
            BEAST_EXPECT(ledata1.size() == 16);
            uint8_t data1[16] = 
            {
                1,1,1,1,1,1,1,1,
                1,1,1,1,1,1,1,2
            };

            for (uint32_t i = 0; i < sizeof(data1); ++i)
                BEAST_EXPECT(data1[i] == ledata1[i]);

            
            uint8_t key2[32] = 
            {
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,1,2,3
            };

            auto const state2 = env.le(ripple::keylet::hookState(aliceid, uint256::fromVoid(key2), beast::zero));

            BEAST_REQUIRE(!state2);
        }

        
        // create a hook state inside the weak side of an execution, while the strong side is rolled back
        {
            TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t state_set (
                uint32_t read_ptr,
                uint32_t read_len,
                uint32_t kread_ptr,
                uint32_t kread_len  
            );
            extern int64_t hook_again(void);
 
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
           
            #define SBUF(x) (uint32_t)(x), sizeof(x)
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                hook_again();

                // create state 
                {

                    uint8_t data[16] = 
                    {
                        0xFFU,1,1,1,1,1,1,1,
                        1,1,1,1,1,1,1,2
                    };


                    uint8_t ff = 0xFFU;

                    ASSERT(state_set(SBUF(data), &ff, 1) == sizeof(data));
                }

                accept(0,0,0);

            }
            )[test.hook]"];


            // install the hook on alice, deleting the other hook
            env(ripple::test::jtx::hook(alice, {{
                {hso(hook, overrideFlag)},
                {},
                {}, 
                {hso_delete()}}}, 0),
                M("set state_set 4"),
                HSFEE);
            env.close();

            // invoke from alice to cho, this will cause a rollback, however the hook state should still be updated
            // because the hook specified hook_again, and in the second weak execution the hook is allowed to
            // set state
            env(pay(alice, cho, XRP(1)), M("test state_set 4 rollback"), fee(XRP(1)), ter(tecHOOK_REJECTED));


            uint8_t key[32] = 
            {
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0xFFU
            };

            auto const state = env.le(ripple::keylet::hookState(aliceid, uint256::fromVoid(key), beast::zero));

            BEAST_EXPECT(state);

            // one hook + two state objects == 3
            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 3);
           

            // delete alice's hook
            env(ripple::test::jtx::hook(alice, {{
                {hso_delete()},
                {},
                {},
                {}}}, 0), 
                M("set state_set 5 delete"),
                HSFEE);
            env.close();

            // check the state is still present
            {
                auto const state = env.le(ripple::keylet::hookState(aliceid, uint256::fromVoid(key), beast::zero));
                BEAST_EXPECT(state);
            }

            // zero hooks + two state objects == 2
            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 2);

            // put on a different hook

            env(ripple::test::jtx::hook(alice, {{hso(rollback_wasm, overrideFlag)}}, 0),
                M("set state_set rollback2"),
                HSFEE);
            env.close();
            
            // check the state is still present
            {
                auto const state = env.le(ripple::keylet::hookState(aliceid, uint256::fromVoid(key), beast::zero));
                BEAST_EXPECT(state);
            }

            // one hooks + two state objects == 3
            BEAST_EXPECT((*env.le("alice"))[sfOwnerCount] == 3);

        }

        // RH TODO:
        // check state can be set on emit callback
        // check reserve - cant make new state object if reserve insufficient
        // try creating many new state objects
        // check namespacing provides for non-collision of same key

    }

    void
    test_sto_emplace()
    {
        testcase("Test sto_emplace");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t sto_emplace (
                uint32_t write_ptr, uint32_t write_len,                                                                            
                uint32_t sread_ptr, uint32_t sread_len,                                                                            
                uint32_t fread_ptr, uint32_t fread_len, uint32_t field_id );
            extern int64_t trace_num(uint32_t, uint32_t, int64_t);
            #define TOO_SMALL -4
            #define TOO_BIG -3
            #define OUT_OF_BOUNDS -1
            #define MEM_OVERLAP -43
            #define PARSE_ERROR -18
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            
            uint8_t sto[] =
            {
                0x11U,0x00U,0x61U,0x22U,0x00U,0x00U,0x00U,0x00U,0x24U,0x04U,0x1FU,0x94U,0xD9U,0x25U,0x04U,0x5EU,
                0x84U,0xB7U,0x2DU,0x00U,0x00U,0x00U,0x00U,0x55U,0x13U,0x40U,0xB3U,0x25U,0x86U,0x31U,0x96U,0xB5U,
                0x6FU,0x41U,0xF5U,0x89U,0xEBU,0x7DU,0x2FU,0xD9U,0x4CU,0x0DU,0x7DU,0xB8U,0x0EU,0x4BU,0x2CU,0x67U,
                0xA7U,0x78U,0x2AU,0xD6U,0xC2U,0xB0U,0x77U,0x50U,0x62U,0x40U,0x00U,0x00U,0x00U,0x00U,0xA4U,0x79U,
                0x94U,0x81U,0x14U,0x37U,0xDFU,0x44U,0x07U,0xE7U,0xAAU,0x07U,0xF1U,0xD5U,0xC9U,0x91U,0xF2U,0xD3U,
                0x6FU,0x9EU,0xB8U,0xC7U,0x34U,0xAFU,0x6CU
            };

            uint8_t ins[] =
            {
                0x56U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,
                0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,
                0x11U,0x11U,0x11U
            };
                    
            uint8_t ans[] = {
                0x11U,0x00U,0x61U,0x22U,0x00U,0x00U,0x00U,0x00U,0x24U,0x04U,0x1FU,0x94U,0xD9U,0x25U,0x04U,
                0x5EU,0x84U,0xB7U,0x2DU,0x00U,0x00U,0x00U,0x00U,0x55U,0x13U,0x40U,0xB3U,0x25U,0x86U,0x31U,
                0x96U,0xB5U,0x6FU,0x41U,0xF5U,0x89U,0xEBU,0x7DU,0x2FU,0xD9U,0x4CU,0x0DU,0x7DU,0xB8U,0x0EU,
                0x4BU,0x2CU,0x67U,0xA7U,0x78U,0x2AU,0xD6U,0xC2U,0xB0U,0x77U,0x50U,0x56U,0x11U,0x11U,0x11U,
                0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,
                0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x62U,
                0x40U,0x00U,0x00U,0x00U,0x00U,0xA4U,0x79U,0x94U,0x81U,0x14U,0x37U,0xDFU,0x44U,0x07U,0xE7U,
                0xAAU,0x07U,0xF1U,0xD5U,0xC9U,0x91U,0xF2U,0xD3U,0x6FU,0x9EU,0xB8U,0xC7U,0x34U,0xAFU,0x6CU
            };

            uint8_t ans2[] = 
            {
                0x11U,0x00U,0x61U,0x22U,0x00U,0x00U,0x00U,0x00U,0x24U,0x04U,0x1FU,0x94U,0xD9U,0x25U,0x04U,
                0x5EU,0x84U,0xB7U,0x2DU,0x00U,0x00U,0x00U,0x00U,0x54U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,
                0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,
                0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x55U,0x13U,0x40U,0xB3U,
                0x25U,0x86U,0x31U,0x96U,0xB5U,0x6FU,0x41U,0xF5U,0x89U,0xEBU,0x7DU,0x2FU,0xD9U,0x4CU,0x0DU,
                0x7DU,0xB8U,0x0EU,0x4BU,0x2CU,0x67U,0xA7U,0x78U,0x2AU,0xD6U,0xC2U,0xB0U,0x77U,0x50U,0x62U,
                0x40U,0x00U,0x00U,0x00U,0x00U,0xA4U,0x79U,0x94U,0x81U,0x14U,0x37U,0xDFU,0x44U,0x07U,0xE7U,
                0xAAU,0x07U,0xF1U,0xD5U,0xC9U,0x91U,0xF2U,0xD3U,0x6FU,0x9EU,0xB8U,0xC7U,0x34U,0xAFU,0x6CU
            };

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t hash[32];

                // Test out of bounds check
                ASSERT(sto_emplace(1000000, 32, 0, 32, 32, 32, 1) == OUT_OF_BOUNDS);
                ASSERT(sto_emplace(0, 1000000, 0, 32, 32, 32, 1) == OUT_OF_BOUNDS);
                ASSERT(sto_emplace(0, 32, 1000000, 32, 32, 32, 1) == OUT_OF_BOUNDS);
                ASSERT(sto_emplace(0, 32, 64, 1000000, 32, 32, 1) == OUT_OF_BOUNDS);
                ASSERT(sto_emplace(0, 32, 64, 32, 1000000, 32, 1) == OUT_OF_BOUNDS);
                ASSERT(sto_emplace(0, 32, 64, 32, 0, 1000000, 1) == OUT_OF_BOUNDS);

                
                // Test size check
                {
                    // write buffer too small
                    ASSERT(sto_emplace(0,1, 0,2, 0,2, 1) == TOO_SMALL);
                    // src buffer too small
                    ASSERT(sto_emplace(0,3, 0,1, 0,2, 1) == TOO_SMALL);
                    // field buffer too small
                    ASSERT(sto_emplace(0,3, 0,2, 0,1, 1) == TOO_SMALL);
                    // field buffer too big
                    ASSERT(sto_emplace(6000, 32000, 0, 5, 5, 6000, 1) == TOO_BIG);
                    // src buffer too big
                    ASSERT(sto_emplace(0, 32000, 32000, 17000, 49000, 4000, 1) == TOO_BIG);
                }


                uint8_t buf[1024];

                // Test overlapping memory
                ASSERT(sto_emplace(buf, 1024, buf+1, 512, 0, 32, 1) == MEM_OVERLAP);
                ASSERT(sto_emplace(buf+1, 1024, buf, 512, 0, 32, 1) == MEM_OVERLAP);
                ASSERT(sto_emplace(0, 700, buf, 512, buf+1, 32, 1) == MEM_OVERLAP);

                // insert ledger index 561111111111111111111111111111111111111111111111111111111111111111

                {

                    ASSERT(sto_emplace(
                                buf, sizeof(buf),
                                sto, sizeof(sto),
                                ins, sizeof(ins), 0x50006U) == 
                            sizeof(sto) + sizeof(ins));
                    
                    for (int i = 0; GUARD(200),  i < sizeof(ans);  ++i)
                        ASSERT(ans[i] == buf[i]);

                    // flip it to 54 and check it is installed before 
                    ins[0] = 0x54U;
                    ASSERT(sto_emplace(
                                buf, sizeof(buf),
                                sto, sizeof(sto),
                                ins, sizeof(ins), 0x50004U) == 
                            sizeof(sto) + sizeof(ins));

                    
                    for (int i = 0; GUARD(200),  i < sizeof(ans2);  ++i)
                        ASSERT(ans2[i] == buf[i]);
                    
                }

                // test front insertion
                {
                    uint8_t sto[] = {0x22U,0x00U,0x00U,0x00U,0x00U};
                    uint8_t ins[] = {0x11U,0x11U,0x11U};
                
                    ASSERT(sto_emplace(buf, sizeof(buf), sto, sizeof(sto), ins, sizeof(ins), 0x10001U) == 
                            sizeof(sto) + sizeof(ins));
                    uint8_t ans[] = {0x11U,0x11U,0x11U,0x22U,0x00U,0x00U,0x00U,0x00U};
                    for (int i = 0; GUARD(10),  i < sizeof(ans);  ++i)
                        ASSERT(ans[i] == buf[i]);
                }
                
                // test back insertion
                {
                    uint8_t sto[] = {0x22U,0x00U,0x00U,0x00U,0x00U};
                    uint8_t ins[] = {0x31U,0x11U,0x11U,0x11U,0x11U,0x12U,0x22U,0x22U,0x22U};
                
                    ASSERT(sto_emplace(buf, sizeof(buf), sto, sizeof(sto), ins, sizeof(ins), 0x30001U) == 
                            sizeof(sto) + sizeof(ins));
                    uint8_t ans[] = {0x22U,0x00U,0x00U,0x00U,0x00U,0x31U,0x11U,0x11U,0x11U,0x11U,0x12U,0x22U,0x22U,
                                     0x22U};
                    for (int i = 0; GUARD(20),  i < sizeof(ans);  ++i)
                        ASSERT(ans[i] == buf[i]);

                }
                    // test replacement
                {
                    uint8_t rep[] = {0x22U,0x10U,0x20U,0x30U,0x40U};
                    ASSERT(sto_emplace(buf, sizeof(buf), sto, sizeof(sto), rep, sizeof(rep), 0x20002U) == 
                            sizeof(sto));

                    // check start
                    ASSERT(buf[0] == sto[0] && buf[1] == sto[1] && buf[2] == sto[2]);

                    // check replaced part
                    for (int i = 3; GUARD(sizeof(rep)+1), i < sizeof(rep)+3; ++i)
                        ASSERT(buf[i] == rep[i-3]);

                    // check end
                    for (int i = sizeof(rep)+3; GUARD(sizeof(sto)),  i < sizeof(sto);  ++i)
                        ASSERT(sto[i] == buf[i]);
                        
                }

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set sto_emplace"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test sto_emplace"), fee(XRP(1)));
    }

    void
    test_sto_erase()
    {
        testcase("Test sto_erase");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t sto_erase (
                uint32_t write_ptr, uint32_t write_len,                                                                            
                uint32_t sread_ptr, uint32_t sread_len,                                                                            
                uint32_t field_id );
            #define TOO_SMALL -4
            #define TOO_BIG -3
            #define OUT_OF_BOUNDS -1
            #define MEM_OVERLAP -43
            #define PARSE_ERROR -18
            #define DOESNT_EXIST -5
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            
            uint8_t sto[] =
            {
                0x11U,0x00U,0x61U,0x22U,0x00U,0x00U,0x00U,0x00U,0x24U,0x04U,0x1FU,0x94U,0xD9U,0x25U,0x04U,0x5EU,
                0x84U,0xB7U,0x2DU,0x00U,0x00U,0x00U,0x00U,0x55U,0x13U,0x40U,0xB3U,0x25U,0x86U,0x31U,0x96U,0xB5U,
                0x6FU,0x41U,0xF5U,0x89U,0xEBU,0x7DU,0x2FU,0xD9U,0x4CU,0x0DU,0x7DU,0xB8U,0x0EU,0x4BU,0x2CU,0x67U,
                0xA7U,0x78U,0x2AU,0xD6U,0xC2U,0xB0U,0x77U,0x50U,0x62U,0x40U,0x00U,0x00U,0x00U,0x00U,0xA4U,0x79U,
                0x94U,0x81U,0x14U,0x37U,0xDFU,0x44U,0x07U,0xE7U,0xAAU,0x07U,0xF1U,0xD5U,0xC9U,0x91U,0xF2U,0xD3U,
                0x6FU,0x9EU,0xB8U,0xC7U,0x34U,0xAFU,0x6CU
            };

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t hash[32];

                // Test out of bounds check
                ASSERT(sto_erase(1000000, 32, 0, 32,  1) == OUT_OF_BOUNDS);
                ASSERT(sto_erase(0, 1000000, 0, 32,  1)  == OUT_OF_BOUNDS);
                ASSERT(sto_erase(0, 32, 1000000, 32,  1) == OUT_OF_BOUNDS);
                ASSERT(sto_erase(0, 32, 64, 1000000,  1) == OUT_OF_BOUNDS);

                
                // Test size check
                {
                    // write buffer too small
                    ASSERT(sto_erase(0,1, 0,2, 1) == TOO_SMALL);
                    ASSERT(sto_erase(0, 32000, 0, 17000,  1) == TOO_BIG);
                }


                uint8_t buf[1024];

                // Test overlapping memory
                ASSERT(sto_erase(buf, 1024, buf+1, 512, 1) == MEM_OVERLAP);
                ASSERT(sto_erase(buf+1, 1024, buf, 512, 1) == MEM_OVERLAP);


                // erase field 22
                {
                    ASSERT(sto_erase(
                                buf, sizeof(buf),
                                sto, sizeof(sto), 0x20002U) == 
                            sizeof(sto) - 5);
                    
                    ASSERT(buf[0] == sto[0] && buf[1] == sto[1] && buf[2] == sto[2]);
                    for (int i = 3; GUARD(sizeof(sto) + 1),  i < sizeof(sto) - 5;  ++i)
                        ASSERT(sto[i+5] == buf[i]);
                }
                
                // test front erasure
                {
                    ASSERT(sto_erase(
                                buf, sizeof(buf),
                                sto, sizeof(sto), 0x10001U) == 
                            sizeof(sto) - 3);
                    
                    for (int i = 3; GUARD(sizeof(sto) + 1),  i < sizeof(sto) - 3;  ++i)
                        ASSERT(sto[i] == buf[i-3]);
                }

                // test back erasure
                {
                    ASSERT(sto_erase(
                                buf, sizeof(buf),
                                sto, sizeof(sto), 0x80001U) == 
                            sizeof(sto) - 22);
                    
                    for (int i = 0; GUARD(sizeof(sto) - 21),  i < sizeof(sto)-22;  ++i)
                        ASSERT(sto[i] == buf[i]);
                }

                // test not found
                {
                    ASSERT(sto_erase(
                                buf, sizeof(buf),
                                sto, sizeof(sto), 0x80002U) == 
                            DOESNT_EXIST);
                    
                    for (int i = 0; GUARD(sizeof(sto) +1),  i < sizeof(sto);  ++i)
                        ASSERT(sto[i] == buf[i]);
                }

                // test total erasure
                {
                    uint8_t rep[] = {0x22U,0x10U,0x20U,0x30U,0x40U};
                    ASSERT(sto_erase(buf, sizeof(buf), rep, sizeof(rep), 0x20002U) == 
                            0);
                        
                }

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set sto_erase"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test sto_erase"), fee(XRP(1)));
    }

    void
    test_sto_subarray()
    {
        testcase("Test sto_subarray");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t sto_subarray(
                uint32_t read_ptr, uint32_t read_len, uint32_t field_id);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define DOESNT_EXIST -5
            #define PARSE_ERROR -18
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            
            uint8_t sto[] =
            {
                0xF4U,0xEBU,0x13U,0x00U,0x01U,0x81U,0x14U,0x20U,0x42U,0x88U,0xD2U,0xE4U,0x7FU,0x8EU,0xF6U,0xC9U,
                0x9BU,0xCCU,0x45U,0x79U,0x66U,0x32U,0x0DU,0x12U,0x40U,0x97U,0x11U,0xE1U,0xEBU,0x13U,0x00U,0x01U,
                0x81U,0x14U,0x3EU,0x9DU,0x4AU,0x2BU,0x8AU,0xA0U,0x78U,0x0FU,0x68U,0x2DU,0x13U,0x6FU,0x7AU,0x56U,
                0xD6U,0x72U,0x4EU,0xF5U,0x37U,0x54U,0xE1U,0xF1U
            };

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t hash[32];

                // Test out of bounds check
                ASSERT(sto_subarray(1000000, 32, 1) == OUT_OF_BOUNDS);
                ASSERT(sto_subarray(0, 1000000, 1) == OUT_OF_BOUNDS);
                
                // Test size check
                ASSERT(sto_subarray(0,1, 1) == TOO_SMALL);

                // Test index 0, should be position 1 length 27
                ASSERT(sto_subarray(sto, sizeof(sto), 0) ==
                    (1ULL << 32ULL) + 27ULL); 

                // Test index 1, should be position 28 length 27
                ASSERT(sto_subarray(sto, sizeof(sto), 1) ==
                    (28ULL << 32ULL) + 27ULL); 
                
                // Test index2, doesn't exist
                ASSERT(sto_subarray(sto, sizeof(sto), 2) == DOESNT_EXIST);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set sto_subarray"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test sto_subarray"), fee(XRP(1)));
    }

    void
    test_sto_subfield()
    {
        testcase("Test sto_subfield");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t sto_subfield(
                uint32_t read_ptr, uint32_t read_len, uint32_t field_id);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define DOESNT_EXIST -5
            #define PARSE_ERROR -18
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            
            uint8_t sto[] =
            {
                0x11U,0x00U,0x53U,0x22U,0x00U,0x00U,0x00U,0x00U,0x25U,0x01U,0x52U,0x70U,0x1AU,0x20U,0x23U,0x00U,
                0x00U,0x00U,0x02U,0x20U,0x26U,0x00U,0x00U,0x00U,0x00U,0x34U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
                0x00U,0x00U,0x55U,0x09U,0xA9U,0xC8U,0x6BU,0xF2U,0x06U,0x95U,0x73U,0x5AU,0xB0U,0x36U,0x20U,0xEBU,
                0x1CU,0x32U,0x60U,0x66U,0x35U,0xACU,0x3DU,0xA0U,0xB7U,0x02U,0x82U,0xF3U,0x7CU,0x67U,0x4FU,0xC8U,
                0x89U,0xEFU,0xE7U
            };

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t hash[32];

                // Test out of bounds check
                ASSERT(sto_subfield(1000000, 32, 1) == OUT_OF_BOUNDS);
                ASSERT(sto_subfield(0, 1000000, 1) == OUT_OF_BOUNDS);
                
                // Test size check
                ASSERT(sto_subfield(0,1, 1) == TOO_SMALL);

                // Test subfield 0x11, should be position 0 length 3, payload pos 1, len 2
                ASSERT(sto_subfield(sto, sizeof(sto),
                     0x10001U) == (1ULL << 32ULL) + 2ULL);
                    
                // Test subfield 0x22, should be position 3 length 5, payload pos 4, len 4
                ASSERT(sto_subfield(sto, sizeof(sto),
                     0x20002U) == (4ULL << 32ULL) + 4ULL);

                // Test subfield 0x34, should be at position 25, length = 9, payload pos 26, len 8
                ASSERT(sto_subfield(sto, sizeof(sto),
                     0x30004U) == (26ULL << 32ULL) + 8ULL);

                // Test final subfield, position 34, length 33, payload pos 35, len 32
                ASSERT(sto_subfield(sto, sizeof(sto),
                     0x50005U) == (35ULL << 32ULL) + 32ULL);

                // Test not found
                ASSERT(sto_subfield(sto, sizeof(sto),
                    0x90009U) == DOESNT_EXIST);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set sto_subfield"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test sto_subfield"), fee(XRP(1)));
    }

    void
    test_sto_validate()
    {
        testcase("Test sto_validate");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const bob = Account{"bob"};
        auto const alice = Account{"alice"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t sto_validate (
                uint32_t read_ptr, uint32_t read_len);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            
            uint8_t sto[] =
            {
                0x11U,0x00U,0x61U,0x22U,0x00U,0x00U,0x00U,0x00U,0x24U,0x04U,0x1FU,0x94U,0xD9U,0x25U,0x04U,0x5EU,
                0x84U,0xB7U,0x2DU,0x00U,0x00U,0x00U,0x00U,0x55U,0x13U,0x40U,0xB3U,0x25U,0x86U,0x31U,0x96U,0xB5U,
                0x6FU,0x41U,0xF5U,0x89U,0xEBU,0x7DU,0x2FU,0xD9U,0x4CU,0x0DU,0x7DU,0xB8U,0x0EU,0x4BU,0x2CU,0x67U,
                0xA7U,0x78U,0x2AU,0xD6U,0xC2U,0xB0U,0x77U,0x50U,0x62U,0x40U,0x00U,0x00U,0x00U,0x00U,0xA4U,0x79U,
                0x94U,0x81U,0x14U,0x37U,0xDFU,0x44U,0x07U,0xE7U,0xAAU,0x07U,0xF1U,0xD5U,0xC9U,0x91U,0xF2U,0xD3U,
                0x6FU,0x9EU,0xB8U,0xC7U,0x34U,0xAFU,0x6CU
            };

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t hash[32];

                // Test out of bounds check
                ASSERT(sto_validate(1000000, 32) == OUT_OF_BOUNDS);
                ASSERT(sto_validate(0, 1000000) == OUT_OF_BOUNDS);
                
                // Test size check
                ASSERT(sto_validate(0,1) == TOO_SMALL);

                // Test validation
                ASSERT(sto_validate(sto, sizeof(sto)) == 1);
                
                // Invalidate
                sto[0] = 0x22U;    
                ASSERT(sto_validate(sto, sizeof(sto)) == 0);

                // Fix                
                sto[0] = 0x11U;
    
                // Invalidate somewhere else
                sto[3] = 0x40U;
                ASSERT(sto_validate(sto, sizeof(sto)) == 0);

                // test small validation
                {
                    uint8_t sto[] = {0x22U,0x00U,0x00U,0x00U,0x00U};
                    ASSERT(sto_validate(sto, sizeof(sto)) == 1);
                }
                
                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set sto_validate"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test sto_validate"), fee(XRP(1)));
    }

    void
    test_trace()
    {
        testcase("Test trace");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t trace (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reservmaed )
            {
                _g(1,1);
                // Test out of bounds check
                ASSERT(trace(1000000, 10, 0, 10, 0) == OUT_OF_BOUNDS);
                ASSERT(trace(0, 1000000, 0, 10, 0) == OUT_OF_BOUNDS);
                ASSERT(trace(0, 10, 1000000, 10, 0) == OUT_OF_BOUNDS);
                ASSERT(trace(0, 10, 0, 1000000, 0) == OUT_OF_BOUNDS);

                ASSERT(trace(0,0,0,0,0) == 0);
                ASSERT(trace(0,1,2,3,1) == 0);

                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set trace"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test trace"), fee(XRP(1)));

    }

    void
    test_trace_float()
    {
        testcase("Test trace_float");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t trace_float (uint32_t, uint32_t, int64_t);
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reservmaed )
            {
                _g(1,1);
                // Test out of bounds check
                ASSERT(trace_float(1000000, 10, 0) == OUT_OF_BOUNDS);
                ASSERT(trace_float(0, 1000000, 0) == OUT_OF_BOUNDS);
                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set trace_float"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test trace_float"), fee(XRP(1)));
    }

    void
    test_trace_num()
    {
        testcase("Test trace_num");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t trace_num (uint32_t, uint32_t, int64_t);
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t r )
            {
                _g(1,1);
                // Test out of bounds check
                ASSERT(trace_num(1000000, 10, 0) == OUT_OF_BOUNDS);
                ASSERT(trace_num(0, 1000000, 0) == OUT_OF_BOUNDS);
                return accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set trace_num"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test trace_num"), fee(XRP(1)));
    }

    void
    test_util_accid()
    {
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t util_accid (uint32_t, uint32_t, uint32_t, uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                uint8_t b[20];
                {
                    const char addr[] = "rMEGJtK2SttrtAfoKaqKUpCrDCi9saNuLg";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0xDEU && b[ 1] == 0x15U && b[ 2] == 0x1EU && b[ 3] == 0x2FU && 
                        b[ 4] == 0xB2U && b[ 5] == 0xAAU && b[ 6] == 0xBDU && b[ 7] == 0x1AU && 
                        b[ 8] == 0x5BU && b[ 9] == 0xD0U && b[10] == 0x2FU && b[11] == 0x63U && 
                        b[12] == 0x68U && b[13] == 0x26U && b[14] == 0xDFU && b[15] == 0x43U && 
                        b[16] == 0x50U && b[17] == 0xC0U && b[18] == 0x40U && b[19] == 0xDEU);
                }
                {
                    const char addr[] = "rNo8xzUAauXENpvsMVJ9Q9w5LtVxCVFN4p";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x97U && b[ 1] == 0x73U && b[ 2] == 0x23U && b[ 3] == 0xAAU && 
                        b[ 4] == 0x33U && b[ 5] == 0x7CU && b[ 6] == 0xB6U && b[ 7] == 0x82U && 
                        b[ 8] == 0x37U && b[ 9] == 0x83U && b[10] == 0x58U && b[11] == 0x3AU && 
                        b[12] == 0x7AU && b[13] == 0xDFU && b[14] == 0x4EU && b[15] == 0xD8U && 
                        b[16] == 0x52U && b[17] == 0x2CU && b[18] == 0xA8U && b[19] == 0xF0U);
                }
                {
                    const char addr[] = "rUpwuJR1xLH18aHLP5nEm4Hw215tmkq6V7";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x78U && b[ 1] == 0xE2U && b[ 2] == 0x10U && b[ 3] == 0xACU && 
                        b[ 4] == 0x98U && b[ 5] == 0x38U && b[ 6] == 0xF2U && b[ 7] == 0x5AU && 
                        b[ 8] == 0x3BU && b[ 9] == 0x7EU && b[10] == 0xDEU && b[11] == 0x51U && 
                        b[12] == 0x37U && b[13] == 0x13U && b[14] == 0x94U && b[15] == 0xEDU && 
                        b[16] == 0x80U && b[17] == 0x77U && b[18] == 0x89U && b[19] == 0x48U);
                }
                {
                    const char addr[] = "ravUPmVUQ65qeuNSFiN6W2U88smjJYHBJm";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x40U && b[ 1] == 0xE8U && b[ 2] == 0x2FU && b[ 3] == 0x55U && 
                        b[ 4] == 0xC7U && b[ 5] == 0x3AU && b[ 6] == 0xEBU && b[ 7] == 0xCFU && 
                        b[ 8] == 0xC9U && b[ 9] == 0x1DU && b[10] == 0x3BU && b[11] == 0xF4U && 
                        b[12] == 0x77U && b[13] == 0x76U && b[14] == 0x50U && b[15] == 0x2BU && 
                        b[16] == 0x49U && b[17] == 0x7BU && b[18] == 0x12U && b[19] == 0x2CU);
                }
                {
                    const char addr[] = "rPXQ8PW1C382oewiEyJrAWtDQBNsQhAtWA";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0xF7U && b[ 1] == 0x13U && b[ 2] == 0x19U && b[ 3] == 0x49U && 
                        b[ 4] == 0x3FU && b[ 5] == 0xA6U && b[ 6] == 0xA3U && b[ 7] == 0xDBU && 
                        b[ 8] == 0x62U && b[ 9] == 0xAEU && b[10] == 0x12U && b[11] == 0x1BU && 
                        b[12] == 0x12U && b[13] == 0x6CU && b[14] == 0xFEU && b[15] == 0x81U && 
                        b[16] == 0x49U && b[17] == 0x5AU && b[18] == 0x49U && b[19] == 0x16U);
                }
                {
                    const char addr[] = "rnZbUT8tpm48KEdfELCxRjJJhNV1JNYcg5";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x32U && b[ 1] == 0x0AU && b[ 2] == 0x5CU && b[ 3] == 0x53U && 
                        b[ 4] == 0x61U && b[ 5] == 0x5BU && b[ 6] == 0x4BU && b[ 7] == 0x57U && 
                        b[ 8] == 0x1DU && b[ 9] == 0xC4U && b[10] == 0x6FU && b[11] == 0x13U && 
                        b[12] == 0xBDU && b[13] == 0x4FU && b[14] == 0x31U && b[15] == 0x70U && 
                        b[16] == 0x84U && b[17] == 0xD1U && b[18] == 0xB1U && b[19] == 0x68U);
                }
                {
                    const char addr[] = "rPghxri3jhBaxBfWGAHrVC4KANoRBe6dcM";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0xF8U && b[ 1] == 0xB6U && b[ 2] == 0x49U && b[ 3] == 0x2BU && 
                        b[ 4] == 0x5BU && b[ 5] == 0x21U && b[ 6] == 0xC8U && b[ 7] == 0xDAU && 
                        b[ 8] == 0xBDU && b[ 9] == 0x0FU && b[10] == 0x1DU && b[11] == 0x2FU && 
                        b[12] == 0xD9U && b[13] == 0xF4U && b[14] == 0x5BU && b[15] == 0xDEU && 
                        b[16] == 0xCCU && b[17] == 0x6AU && b[18] == 0xEBU && b[19] == 0x91U);
                }
                {
                    const char addr[] = "r4Tck2QJcfcwBuTgVJXYb4QbrKP6mT1acM";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0xEBU && b[ 1] == 0x63U && b[ 2] == 0x4CU && b[ 3] == 0xD6U && 
                        b[ 4] == 0xF9U && b[ 5] == 0xBFU && b[ 6] == 0x50U && b[ 7] == 0xC1U && 
                        b[ 8] == 0xD9U && b[ 9] == 0x79U && b[10] == 0x30U && b[11] == 0x84U && 
                        b[12] == 0x1BU && b[13] == 0xFCU && b[14] == 0x35U && b[15] == 0x32U && 
                        b[16] == 0xBDU && b[17] == 0x6DU && b[18] == 0xC0U && b[19] == 0x75U);
                }
                {
                    const char addr[] = "rETHUL5T1SzM6AMotnsK5V3J5XMwJ9UhZ2";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x9EU && b[ 1] == 0x8AU && b[ 2] == 0x18U && b[ 3] == 0x66U && 
                        b[ 4] == 0x92U && b[ 5] == 0x0EU && b[ 6] == 0xE5U && b[ 7] == 0xEDU && 
                        b[ 8] == 0xFAU && b[ 9] == 0xE3U && b[10] == 0x23U && b[11] == 0x15U && 
                        b[12] == 0xCBU && b[13] == 0x83U && b[14] == 0xEFU && b[15] == 0x73U && 
                        b[16] == 0xE4U && b[17] == 0x91U && b[18] == 0x0BU && b[19] == 0xCAU);
                }
                {
                    const char addr[] = "rh9CggaWiY6QdD55ZkbbnrFpHJkKSauLfC";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x22U && b[ 1] == 0x8BU && b[ 2] == 0xFFU && b[ 3] == 0x31U && 
                        b[ 4] == 0xB4U && b[ 5] == 0x93U && b[ 6] == 0xF6U && b[ 7] == 0xC1U && 
                        b[ 8] == 0x12U && b[ 9] == 0xEAU && b[10] == 0xD6U && b[11] == 0xDFU && 
                        b[12] == 0xC4U && b[13] == 0x05U && b[14] == 0xB3U && b[15] == 0x7DU && 
                        b[16] == 0xC0U && b[17] == 0x65U && b[18] == 0x21U && b[19] == 0x34U);
                }
                {
                    const char addr[] = "r9sYGdPCGuJauy8QVG4CHnvp5U4eu3yY2B";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x58U && b[ 1] == 0x3BU && b[ 2] == 0xF0U && b[ 3] == 0xCBU && 
                        b[ 4] == 0x95U && b[ 5] == 0x80U && b[ 6] == 0xDEU && b[ 7] == 0xA0U && 
                        b[ 8] == 0xB3U && b[ 9] == 0x71U && b[10] == 0xD0U && b[11] == 0x18U && 
                        b[12] == 0x17U && b[13] == 0x1AU && b[14] == 0xBBU && b[15] == 0x98U && 
                        b[16] == 0x1FU && b[17] == 0xCCU && b[18] == 0x7CU && b[19] == 0x68U);
                }
                {
                    const char addr[] = "r4yJX9eU65WHfmKz6xXmSRf9CZN6bXfpWb";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0xF1U && b[ 1] == 0x00U && b[ 2] == 0x8FU && b[ 3] == 0x64U && 
                        b[ 4] == 0x0FU && b[ 5] == 0x99U && b[ 6] == 0x19U && b[ 7] == 0xDAU && 
                        b[ 8] == 0xCFU && b[ 9] == 0x48U && b[10] == 0x18U && b[11] == 0x1CU && 
                        b[12] == 0x35U && b[13] == 0x2EU && b[14] == 0xE4U && b[15] == 0x3EU && 
                        b[16] == 0x37U && b[17] == 0x7CU && b[18] == 0x01U && b[19] == 0xF6U);
                }
                {
                    const char addr[] = "rBkXoWoXPHuZy2nHbE7L1zJfqAvb4jHRrK";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x75U && b[ 1] == 0xECU && b[ 2] == 0xDBU && b[ 3] == 0x3BU && 
                        b[ 4] == 0x9AU && b[ 5] == 0x71U && b[ 6] == 0xD9U && b[ 7] == 0xEFU && 
                        b[ 8] == 0xD6U && b[ 9] == 0x55U && b[10] == 0x15U && b[11] == 0xDDU && 
                        b[12] == 0xEAU && b[13] == 0xD2U && b[14] == 0x36U && b[15] == 0x7AU && 
                        b[16] == 0x05U && b[17] == 0x6FU && b[18] == 0x4EU && b[19] == 0x5FU);
                }
                {
                    const char addr[] = "rnaUBeEBNuyv57Jk127DsApEQoR8JqWpie";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x2CU && b[ 1] == 0xDBU && b[ 2] == 0xEBU && b[ 3] == 0x1FU && 
                        b[ 4] == 0x5EU && b[ 5] == 0xC5U && b[ 6] == 0xD7U && b[ 7] == 0x5FU && 
                        b[ 8] == 0xACU && b[ 9] == 0xBDU && b[10] == 0x19U && b[11] == 0xC8U && 
                        b[12] == 0x3FU && b[13] == 0x45U && b[14] == 0x3BU && b[15] == 0xA8U && 
                        b[16] == 0xA0U && b[17] == 0x1CU && b[18] == 0xDBU && b[19] == 0x0FU);
                }
                {
                    const char addr[] = "rJHmUPMQ6qYdaqMizDZY8FKcCqCJxYYnb3";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0xBDU && b[ 1] == 0xA5U && b[ 2] == 0xAFU && b[ 3] == 0xDAU && 
                        b[ 4] == 0x5FU && b[ 5] == 0x04U && b[ 6] == 0xE7U && b[ 7] == 0xEFU && 
                        b[ 8] == 0x16U && b[ 9] == 0x7AU && b[10] == 0x35U && b[11] == 0x94U && 
                        b[12] == 0x6EU && b[13] == 0xEFU && b[14] == 0x19U && b[15] == 0xFAU && 
                        b[16] == 0x12U && b[17] == 0xF3U && b[18] == 0x1CU && b[19] == 0x64U);
                }
                {
                    const char addr[] = "rpJtt64FNNtaEBgqbJcrrunucUWJSdKJa2";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x0EU && b[ 1] == 0x5AU && b[ 2] == 0x83U && b[ 3] == 0x89U && 
                        b[ 4] == 0xC0U && b[ 5] == 0x5EU && b[ 6] == 0x56U && b[ 7] == 0xD1U && 
                        b[ 8] == 0x50U && b[ 9] == 0xBCU && b[10] == 0x45U && b[11] == 0x7BU && 
                        b[12] == 0x86U && b[13] == 0x46U && b[14] == 0xF1U && b[15] == 0xCFU && 
                        b[16] == 0xB7U && b[17] == 0xD0U && b[18] == 0xBFU && b[19] == 0xD4U);
                }
                {
                    const char addr[] = "rUC2XjZURBYQ8r6i5sqWnhtDmFFdJFobb9";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x7FU && b[ 1] == 0xF5U && b[ 2] == 0x2DU && b[ 3] == 0xF4U && 
                        b[ 4] == 0x98U && b[ 5] == 0x2BU && b[ 6] == 0x7CU && b[ 7] == 0x14U && 
                        b[ 8] == 0x7EU && b[ 9] == 0x9AU && b[10] == 0x8BU && b[11] == 0xEBU && 
                        b[12] == 0x1AU && b[13] == 0x53U && b[14] == 0x60U && b[15] == 0x34U && 
                        b[16] == 0x95U && b[17] == 0x42U && b[18] == 0x4AU && b[19] == 0x44U);
                }
                {
                    const char addr[] = "rKEsw1ExpKaukXyyPCxeZdAF5V68kPSAVZ";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0xC8U && b[ 1] == 0x19U && b[ 2] == 0xE6U && b[ 3] == 0x2AU && 
                        b[ 4] == 0xDDU && b[ 5] == 0x42U && b[ 6] == 0x48U && b[ 7] == 0xD6U && 
                        b[ 8] == 0x7DU && b[ 9] == 0xA5U && b[10] == 0x56U && b[11] == 0x66U && 
                        b[12] == 0x55U && b[13] == 0xB4U && b[14] == 0xBFU && b[15] == 0xDEU && 
                        b[16] == 0x99U && b[17] == 0xCFU && b[18] == 0xEDU && b[19] == 0x96U);
                }
                {
                    const char addr[] = "rEXhVGVWdte28r1DUzfgKLjNiHi1Tn6R7X";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x9FU && b[ 1] == 0x41U && b[ 2] == 0x26U && b[ 3] == 0xA3U && 
                        b[ 4] == 0x6DU && b[ 5] == 0x56U && b[ 6] == 0x01U && b[ 7] == 0xC8U && 
                        b[ 8] == 0x09U && b[ 9] == 0x63U && b[10] == 0x76U && b[11] == 0xEDU && 
                        b[12] == 0x4CU && b[13] == 0x45U && b[14] == 0x66U && b[15] == 0x63U && 
                        b[16] == 0x16U && b[17] == 0xC9U && b[18] == 0x5CU && b[19] == 0x80U);
                }
                {
                    const char addr[] = "r3TcfPNEvidJ2LkNoFojffcCd7RgT53Thg";
                    ASSERT(20 ==
                        util_accid((uint32_t)b, 20, (uint32_t)addr, sizeof(addr)));
                    ASSERT(
                        b[ 0] == 0x51U && b[ 1] == 0xD1U && b[ 2] == 0x00U && b[ 3] == 0xFFU && 
                        b[ 4] == 0x0DU && b[ 5] == 0x92U && b[ 6] == 0x18U && b[ 7] == 0x73U && 
                        b[ 8] == 0x80U && b[ 9] == 0x30U && b[10] == 0xC5U && b[11] == 0x1AU && 
                        b[12] == 0xF2U && b[13] == 0x9FU && b[14] == 0x52U && b[15] == 0x8EU && 
                        b[16] == 0xB8U && b[17] == 0x63U && b[18] == 0x08U && b[19] == 0x7CU);
                }

                // Test out of bounds check
                ASSERT(util_accid(1000000, 20, 0, 35) == OUT_OF_BOUNDS);
                ASSERT(util_accid(0, 35, 10000000, 20) == OUT_OF_BOUNDS);
                ASSERT(util_accid(0, 19, 0, 0) == TOO_SMALL);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set util_accid"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test util_accid"), fee(XRP(1)));
    }

    void
    test_util_keylet()
    {
        testcase("Test util_keylet");
        using namespace jtx;

        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        auto const a = alice.id();
        auto const b = bob.id();

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t util_keylet (
                uint32_t write_ptr,
                uint32_t write_len,
                uint32_t keylet_type,
                uint32_t a,
                uint32_t b,
                uint32_t c,
                uint32_t d,
                uint32_t e,
                uint32_t f
            );
            #define OUT_OF_BOUNDS -1
            #define INVALID_ARGUMENT -7
            #define TOO_SMALL -4
            #define KEYLET_HOOK 1
            #define KEYLET_HOOK_STATE 2
            #define KEYLET_ACCOUNT 3
            #define KEYLET_AMENDMENTS 4
            #define KEYLET_CHILD 5
            #define KEYLET_SKIP 6
            #define KEYLET_FEES 7
            #define KEYLET_NEGATIVE_UNL 8
            #define KEYLET_LINE 9
            #define KEYLET_OFFER 10
            #define KEYLET_QUALITY 11
            #define KEYLET_EMITTED_DIR 12
            #define KEYLET_SIGNERS 14
            #define KEYLET_CHECK 15
            #define KEYLET_DEPOSIT_PREAUTH 16
            #define KEYLET_UNCHECKED 17
            #define KEYLET_OWNER_DIR 18
            #define KEYLET_PAGE 19
            #define KEYLET_ESCROW 20
            #define KEYLET_PAYCHAN 21
            #define KEYLET_EMITTED_TXN 22
            #define KEYLET_NFT_OFFER 23
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            #define ASSERT_KL_EQ(b)\
            {\
                uint64_t* n = (uint64_t*)(b);\
                uint64_t* m = (uint64_t*)(buf);\
                ASSERT(n[0] == m[0] && n[1] == m[1] && n[2] == m[2] && n[3] == m[3]);\
            } 
            #define SBUF(x) x,sizeof(x)
            //C5D0F34B0A1905BC3B29AA1BE139FE04D60C8694D3950A8D80251D10B563A822
            uint8_t ns[] = 
            {
                0xC5U,0xD0U,0xF3U,0x4BU,0x0AU,0x19U,0x05U,0xBCU,0x3BU,0x29U,0xAAU,0x1BU,0xE1U,
                0x39U,0xFEU,0x04U,0xD6U,0x0CU,0x86U,0x94U,0xD3U,0x95U,0x0AU,0x8DU,0x80U,0x25U,
                0x1DU,0x10U,0xB5U,0x63U,0xA8U,0x22U
            };

            //2D0CB3CD60DA33B5AA7FEA321F111663EAED32481C6B700E484550F45AD96223
            uint8_t klkey[] =
            {
                0x00U, 0x00U, 
                0x2DU,0x0CU,0xB3U,0xCDU,0x60U,0xDAU,0x33U,0xB5U,0xAAU,0x7FU,0xEAU,0x32U,0x1FU,
                0x11U,0x16U,0x63U,0xEAU,0xEDU,0x32U,0x48U,0x1CU,0x6BU,0x70U,0x0EU,0x48U,0x45U,
                0x50U,0xF4U,0x5AU,0xD9U,0x62U,0x23U
            };

            uint8_t cur[] =
            {
                0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,
                0x00U,0x00U,0x55U,0x53U,0x44U,0x00U,0x00U,0x00U,0x00U,0x00U
                
            };

            uint8_t* key = klkey + 2;

            uint8_t a[] = //rB6v18pQ765Z9DH5RQsTFevoQPFmRtBqhT
            {
                0x75U,0x6EU,0xDEU,0x88U,0xA9U,0x07U,0xD4U,0xCCU,0xF3U,0x8DU,0x6AU,0xDBU,
                0x9FU,0xC7U,0x94U,0x64U,0x19U,0xF0U,0xC4U,0x1DU
            };

            uint8_t b[] = //raKM1bZkGmASBqN5v2swrf2uAPJ32Cd8GV
            {
                0x3AU,0x51U,0x8AU,0x22U,0x53U,0x81U,0x60U,0x84U,0x1CU,0x14U,0x32U,0xFEU,
                0x6FU,0x3EU,0x6DU,0x6EU,0x76U,0x29U,0xFBU,0xBAU
            };

            int64_t hook(uint32_t reserved )
            {
                _g(1,1);
                uint8_t buf[34];
                int64_t e = 0;


                // Test out of bounds check
                ASSERT(util_keylet(1000000, 34, KEYLET_SKIP, 0,0,0,0,0,0) == OUT_OF_BOUNDS);
                ASSERT(util_keylet((uint32_t)buf, 1000000, KEYLET_SKIP, 0,0,0,0,0,0) == OUT_OF_BOUNDS);

                // Test min size
                ASSERT(util_keylet((uint32_t)buf, 33, KEYLET_SKIP, 0,0,0,0,0,0) == TOO_SMALL);


                // Test one of each type
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_HOOK,
                    SBUF(a),
                    0,0,0,0
                )));
                
                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x48U,0x6CU,0x4BU,0x29U,0xC6U,0x0FU,0x40U,0x5DU,0xB7U,0x6EU,0x87U,
                        0x65U,0x4AU,0x2FU,0x15U,0x4BU,0xABU,0x99U,0xC7U,0x62U,0x29U,0x80U,0x10U,
                        0xA1U,0x89U,0x78U,0x52U,0x90U,0x80U,0x2FU,0x78U,0xBDU,0xCCU
                    };
                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_HOOK_STATE,
                    SBUF(b), key, 32, SBUF(ns)
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x76U,0x28U,0xAFU,0xCCU,0x25U,0x0AU,0x64U,0x41U,0x8EU,0xB7U,0x83U,
                        0x68U,0xEBU,0x4EU,0xC5U,0x52U,0x4AU,0xEBU,0x97U,0x54U,0xABU,0xC1U,0x0BU,
                        0x13U,0x06U,0x7FU,0xFBU,0x9FU,0x4BU,0xD8U,0x38U,0x62U,0xF2U
                    }; 
                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_ACCOUNT,
                    SBUF(b),
                    0,0,0,0
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x61U,0xC6U,0x55U,0xDDU,0x8DU,0x8EU,0xD3U,0xBAU,0xB4U,0xA0U,0xF1U,
                        0xECU,0x2DU,0xA9U,0x99U,0xF4U,0x1BU,0xA6U,0x82U,0xC6U,0x84U,0xF9U,0x99U,
                        0x66U,0xB9U,0x3CU,0x9AU,0xC3U,0xE3U,0x5CU,0x9AU,0x81U,0x6DU
                    };
                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_AMENDMENTS,
                    0,0,0,0,0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x66U,0x7DU,0xB0U,0x78U,0x8CU,0x02U,0x0FU,0x02U,0x78U,0x0AU,0x67U,
                        0x3DU,0xC7U,0x47U,0x57U,0xF2U,0x38U,0x23U,0xFAU,0x30U,0x14U,0xC1U,0x86U,
                        0x6EU,0x72U,0xCCU,0x4CU,0xD8U,0xB2U,0x26U,0xCDU,0x6EU,0xF4U
                    };
                    ASSERT_KL_EQ(ans);
                }

                
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_CHILD,
                    key, 32,
                    0,0,0,0
                )));

                {
                    klkey[0] = 0x1CU;
                    klkey[1] = 0xD2U;
                    ASSERT_KL_EQ(klkey);
                }
                

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_SKIP,
                    0,0,0,0,0,0
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x68U,0xB4U,0x97U,0x9AU,0x36U,0xCDU,0xC7U,0xF3U,0xD3U,
                        0xD5U,0xC3U,0x1AU,0x4EU,0xAEU,0x2AU,0xC7U,0xD7U,0x20U,0x9DU,
                        0xDAU,0x87U,0x75U,0x88U,0xB9U,0xAFU,0xC6U,0x67U,0x99U,0x69U,
                        0x2AU,0xB0U,0xD6U,0x6BU
                    };

                    ASSERT_KL_EQ(ans);
                }


                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_FEES,
                    0,0,0,0,0,0
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x73U,0x4BU,0xC5U,0x0CU,0x9BU,0x0DU,0x85U,0x15U,0xD3U,
                        0xEAU,0xAEU,0x1EU,0x74U,0xB2U,0x9AU,0x95U,0x80U,0x43U,0x46U,
                        0xC4U,0x91U,0xEEU,0x1AU,0x95U,0xBFU,0x25U,0xE4U,0xAAU,0xB8U,
                        0x54U,0xA6U,0xA6U,0x51U
                    };

                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_NEGATIVE_UNL,
                    0,0,0,0,0,0
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x4EU,0x2EU,0x8AU,0x59U,0xAAU,0x9DU,0x3BU,0x5BU,0x18U,
                        0x6BU,0x0BU,0x9EU,0x0FU,0x62U,0xE6U,0xC0U,0x25U,0x87U,0xCAU,
                        0x74U,0xA4U,0xD7U,0x78U,0x93U,0x8EU,0x95U,0x7BU,0x63U,0x57U,
                        0xD3U,0x64U,0xB2U,0x44U
                    };
                    ASSERT_KL_EQ(ans);
                }

                
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_LINE,
                    SBUF(a),
                    SBUF(b),
                    SBUF(cur)
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x72U,0x0EU,0xB8U,0x2AU,0xDDU,0x5EU,0x15U,0x59U,0x1BU,
                        0xF6U,0xE3U,0x6DU,0xBCU,0x3CU,0x12U,0xD3U,0x07U,0x6DU,0x43U,
                        0xA8U,0x53U,0xF8U,0xF9U,0xE8U,0xA7U,0xD8U,0x4FU,0xE1U,0xE9U,
                        0x7AU,0x2AU,0xC7U,0x3DU
                    };
                    ASSERT_KL_EQ(ans);
                }
                
                // test 3 byte code
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_LINE,
                    SBUF(a),
                    SBUF(b),
                    (uint32_t)"USD", 3
                )));

                {
                    // same answer
                    uint8_t ans[] =
                    {
                        0x00U,0x72U,0x0EU,0xB8U,0x2AU,0xDDU,0x5EU,0x15U,0x59U,0x1BU,
                        0xF6U,0xE3U,0x6DU,0xBCU,0x3CU,0x12U,0xD3U,0x07U,0x6DU,0x43U,
                        0xA8U,0x53U,0xF8U,0xF9U,0xE8U,0xA7U,0xD8U,0x4FU,0xE1U,0xE9U,
                        0x7AU,0x2AU,0xC7U,0x3DU
                    };
                    ASSERT_KL_EQ(ans);
                }

                // test invalid 3 byte code
                ASSERT(INVALID_ARGUMENT ==
                     util_keylet(buf, 34, KEYLET_LINE, SBUF(a), SBUF(b), (uint32_t)"`SD", 3));

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_OFFER,
                    SBUF(a), 
                    1,
                    0,0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x6FU,0x60U,0x14U,0x48U,0x80U,0x97U,0x5FU,0x76U,0x6AU,
                        0xB2U,0x2CU,0x32U,0x2FU,0x10U,0x8EU,0x03U,0x43U,0x51U,0xDEU,
                        0x89U,0x6CU,0xF4U,0x9FU,0x6BU,0x4AU,0xC7U,0x2CU,0x54U,0xF7U,
                        0x27U,0x29U,0x9BU,0xE8U
                    };
                    ASSERT_KL_EQ(ans);
                }


                // again with a uint256
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_OFFER,
                    SBUF(a), 
                    SBUF(ns),
                    0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x6FU,0x23U,0x61U,0x7FU,0x44U,0x91U,0x1CU,0xBAU,0x3BU,
                        0x5CU,0xBEU,0xE9U,0x42U,0x22U,0xACU,0xA4U,0x29U,0xF4U,0xD6U,
                        0x60U,0x01U,0xA8U,0xABU,0x9BU,0x98U,0x5EU,0xB8U,0xB8U,0x42U,
                        0x9FU,0x1EU,0x91U,0x4BU
                    };
                    ASSERT_KL_EQ(ans);
                }

                // verify that quality returns invalid argument when passed
                // something that isn't a dir keylet
                klkey[0] = 0;
                klkey[1] = 0x65U;
                ASSERT(INVALID_ARGUMENT == (e=util_keylet(buf, 34, KEYLET_QUALITY,
                    SBUF(klkey),
                    0,1,
                    0,0
                )));

                // now change it to a dir
                klkey[1] = 0x64U;
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_QUALITY,
                    SBUF(klkey),
                    0,1,
                    0,0
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x64U,0x2DU,0x0CU,0xB3U,0xCDU,0x60U,0xDAU,0x33U,0xB5U,
                        0xAAU,0x7FU,0xEAU,0x32U,0x1FU,0x11U,0x16U,0x63U,0xEAU,0xEDU,
                        0x32U,0x48U,0x1CU,0x6BU,0x70U,0x0EU,0x00U,0x00U,0x00U,0x00U,
                        0x00U,0x00U,0x00U,0x01U
                    };
                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_EMITTED_DIR,
                    0,0,0,0,0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x64U,0xB4U,0xDEU,0x82U,0x30U,0x55U,0xD0U,0x0BU,0xC1U,
                        0x2CU,0xD7U,0x8FU,0xE1U,0xAAU,0xF7U,0x4EU,0xE6U,0x06U,0x21U,
                        0x95U,0xB2U,0x62U,0x9FU,0x49U,0xA2U,0x59U,0x15U,0xA3U,0x9CU,
                        0x64U,0xBEU,0x19U,0x00U
                    };
                    ASSERT_KL_EQ(ans);
                }


                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_SIGNERS,
                    SBUF(a),
                    0,0,0,0
                )));
                {
                    uint8_t ans[] =
                    {
                        0x00U,0x53U,0xDFU,0x8FU,0xF0U,0xCEU,0x41U,0x1AU,0x3BU,0x8FU,
                        0x1BU,0xB5U,0xBBU,0x32U,0x78U,0x17U,0x15U,0xD6U,0x77U,0x42U,
                        0xF5U,0xB5U,0x63U,0xB8U,0x77U,0xB3U,0x3BU,0x07U,0x76U,0xF6U,
                        0xF7U,0xBCU,0xDAU,0x1DU
                    };
                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_CHECK,
                    SBUF(a), 1, 0,
                    0,0
                )));
                {
                    uint8_t ans[] =
                    {
                        0x00U,0x43U,0x08U,0x1FU,0x26U,0xFFU,0x79U,0x1AU,0xF7U,0x54U,
                        0x26U,0xEDU,0xF9U,0xEBU,0x08U,0x44U,0x85U,0x28U,0x58U,0x2CU,
                        0xB1U,0xA4U,0xEFU,0x4FU,0xD0U,0xB4U,0x49U,0x9BU,0x76U,0x82U,
                        0xE7U,0x69U,0xA6U,0xB5U
                    };
                    ASSERT_KL_EQ(ans);
                }

                // ans again with uint256
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_CHECK,
                    SBUF(a), SBUF(ns),
                    0,0
                )));
                {
                    uint8_t ans[] =
                    {
                        0x00U,0x43U,0x94U,0xE3U,0x6FU,0x0DU,0xD3U,0xEDU,0xC0U,0x2CU,
                        0x49U,0xA5U,0xAAU,0x0EU,0xCCU,0x49U,0x18U,0x39U,0x92U,0xABU,
                        0x57U,0xC3U,0x2DU,0x9EU,0x45U,0x51U,0x04U,0x78U,0x49U,0x49U,
                        0xD1U,0xE6U,0xD2U,0x01U
                    };
                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_DEPOSIT_PREAUTH,
                    SBUF(a), SBUF(b),
                    0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x70U,0x88U,0x90U,0x0FU,0x27U,0x66U,0x57U,0xBCU,0xC0U,
                        0x5DU,0xA1U,0x67U,0x40U,0xABU,0x9DU,0x33U,0x01U,0x8EU,0x45U,
                        0x71U,0x7BU,0x0EU,0xC4U,0x2EU,0x4DU,0x11U,0xBDU,0x6DU,0xBDU,
                        0x94U,0x03U,0x48U,0xE0U
                    };
                    ASSERT_KL_EQ(ans);
                }

                klkey[0] = 0;
                klkey[1] = 0;
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_UNCHECKED,
                    key, 32,
                    0,0,0,0
                )));

                ASSERT_KL_EQ(klkey);
                
                
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_OWNER_DIR,
                    SBUF(a),
                    0,0,0,0
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x64U,0xC8U,0x5EU,0x01U,0x29U,0x06U,0x7BU,0x75U,0xADU,
                        0x30U,0xB0U,0xAAU,0x1CU,0xC2U,0x5BU,0x0AU,0x82U,0xC7U,0xF9U,
                        0xAAU,0xBDU,0xEEU,0x05U,0xFFU,0x01U,0x66U,0x69U,0xEFU,0x9DU,
                        0x82U,0xDCU,0xECU,0x30U
                    };
                    ASSERT_KL_EQ(ans);
                }

                
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_PAGE,
                    SBUF(ns), 0, 1,
                    0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x64U,0x61U,0xE6U,0x05U,0x1AU,0xB0U,0x49U,0x89U,0x2EU,
                        0x75U,0xC9U,0x3DU,0x67U,0xFBU,0x7AU,0x63U,0xF1U,0xEFU,0x56U,
                        0xDDU,0xAFU,0x3EU,0x6BU,0x43U,0x6FU,0x57U,0x6EU,0x8CU,0x01U,
                        0x81U,0x98U,0x2EU,0x48U
                    };
                    ASSERT_KL_EQ(ans);
                }


                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_ESCROW,
                    SBUF(a), 1, 0,
                    0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x75U,0x13U,0xEFU,0x04U,0xCDU,0x33U,0x6AU,0xADU,0xF6U,
                        0x3DU,0x0CU,0x7EU,0x05U,0x6CU,0x84U,0x9AU,0x7CU,0xF6U,0x72U,
                        0x5EU,0x99U,0xBCU,0x93U,0x80U,0x1EU,0xF5U,0x78U,0xA0U,0x32U,
                        0x72U,0x5BU,0x84U,0xFEU
                    };
                    ASSERT_KL_EQ(ans);
                }
                
                // again with a uint256
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_ESCROW,
                    SBUF(a), 
                    SBUF(ns),
                    0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x75U,0xC1U,0xC6U,0xC5U,0x23U,0x74U,0x87U,0x12U,0x56U,
                        0xAAU,0x7AU,0x1FU,0xB3U,0x29U,0x7AU,0x0AU,0x55U,0x88U,0x7DU,
                        0x16U,0x6AU,0xCFU,0x85U,0x28U,0x59U,0x88U,0xC2U,0xDAU,0x81U,
                        0x7FU,0x03U,0x90U,0x43U
                    };
                    ASSERT_KL_EQ(ans);
                }
                
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_PAYCHAN,
                    SBUF(a), SBUF(b), 1, 0
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x78U,0xEDU,0x04U,0xCEU,0x27U,0x20U,0x21U,0x55U,0x2BU,
                        0xBFU,0xA1U,0xE5U,0xFFU,0xBBU,0x53U,0xB6U,0x45U,0xA2U,0xFFU,
                        0x8AU,0x44U,0x66U,0xD5U,0x76U,0x24U,0xB5U,0x71U,0xE6U,0x44U,
                        0x9EU,0xEBU,0xFCU,0x5AU
                    };
                    ASSERT_KL_EQ(ans);
                }


                // again with uint256
                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_PAYCHAN,
                    SBUF(a), SBUF(b), SBUF(ns)
                )));

                {
                    uint8_t ans[] = 
                    {
                        0x00U,0x78U,0x7DU,0xE1U,0x01U,0xF6U,0x2BU,0xB0U,0x55U,0x80U,
                        0xB9U,0xD6U,0xB0U,0x3FU,0x3BU,0xB0U,0x01U,0xBDU,0xE6U,0x9BU,
                        0x89U,0x0FU,0x8AU,0xCDU,0xBEU,0x71U,0x73U,0x5EU,0xC3U,0x63U,
                        0xF8U,0xC5U,0x4BU,0x9BU
                    };
                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_EMITTED_TXN,
                    ns, 32,
                    0,0,0,0
                )));

                {
                    uint8_t ans[] =
                    {
                        0x00U,0x45U,0xF3U,0x51U,0x2DU,0x1CU,0x80U,0xA3U,0xC0U,0xB1U,
                        0x46U,0x04U,0xE1U,0xADU,0xDBU,0x90U,0x1CU,0x66U,0x32U,0x10U,
                        0x08U,0xCCU,0xD0U,0xABU,0xD2U,0xDBU,0xBEU,0xC4U,0x08U,0xA6U,
                        0x0FU,0x6AU,0x62U,0xE9U
                    };
                    ASSERT_KL_EQ(ans);
                }

                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_NFT_OFFER,
                    SBUF(a), 1, 0,
                    0,0
                )));


                ASSERT(34 == (e=util_keylet(buf, 34, KEYLET_NFT_OFFER,
                    SBUF(a), SBUF(ns),
                    0,0
                )));

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set util_keylet"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test util_keylet"), fee(XRP(1)));
    }

    void
    test_util_raddr()
    {
        testcase("Test util_raddr");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t util_raddr (uint32_t, uint32_t, uint32_t, uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                {
                    uint8_t raw[20] = {
                        0x6BU, 0x30U, 0xE2U, 0x94U, 0xF3U, 0x40U, 0x3FU, 0xF8U,
                        0x7CU, 0xEFU, 0x9EU, 0x72U, 0x21U, 0x7FU, 0xF7U, 0xEBU,
                        0x4AU, 0x6AU, 0x43U, 0xF4U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x77U && addr[ 2] == 0x6DU && addr[ 3] == 0x6DU &&
                        addr[ 4] == 0x31U && addr[ 5] == 0x33U && addr[ 6] == 0x70U && addr[ 7] == 0x37U &&
                        addr[ 8] == 0x56U && addr[ 9] == 0x67U && addr[10] == 0x36U && addr[11] == 0x4BU &&
                        addr[12] == 0x6DU && addr[13] == 0x6EU && addr[14] == 0x71U && addr[15] == 0x4BU &&
                        addr[16] == 0x52U && addr[17] == 0x77U && addr[18] == 0x44U && addr[19] == 0x7AU &&
                        addr[20] == 0x78U && addr[21] == 0x76U && addr[22] == 0x69U && addr[23] == 0x35U &&
                        addr[24] == 0x58U && addr[25] == 0x70U && addr[26] == 0x36U && addr[27] == 0x77U &&
                        addr[28] == 0x6EU && addr[29] == 0x48U && addr[30] == 0x4DU && addr[31] == 0x44U &&
                        addr[32] == 0x44U && addr[33] == 0x68U);
                }
                {
                    uint8_t raw[20] = {
                        0xE4U, 0x0FU, 0xA3U, 0x4EU, 0x3EU, 0x66U, 0x15U, 0x36U,
                        0x64U, 0x89U, 0x4FU, 0xCBU, 0xFBU, 0xFCU, 0xFEU, 0x2DU,
                        0x2DU, 0x19U, 0x0DU, 0x69U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x4DU && addr[ 2] == 0x38U && addr[ 3] == 0x31U &&
                        addr[ 4] == 0x6FU && addr[ 5] == 0x48U && addr[ 6] == 0x77U && addr[ 7] == 0x68U &&
                        addr[ 8] == 0x37U && addr[ 9] == 0x35U && addr[10] == 0x39U && addr[11] == 0x34U &&
                        addr[12] == 0x6AU && addr[13] == 0x48U && addr[14] == 0x38U && addr[15] == 0x70U &&
                        addr[16] == 0x36U && addr[17] == 0x31U && addr[18] == 0x57U && addr[19] == 0x65U &&
                        addr[20] == 0x31U && addr[21] == 0x73U && addr[22] == 0x64U && addr[23] == 0x58U &&
                        addr[24] == 0x46U && addr[25] == 0x42U && addr[26] == 0x35U && addr[27] == 0x48U &&
                        addr[28] == 0x52U && addr[29] == 0x52U && addr[30] == 0x79U && addr[31] == 0x4BU &&
                        addr[32] == 0x76U && addr[33] == 0x4AU);
                }
                {
                    uint8_t raw[20] = {
                        0x0CU, 0x90U, 0x4BU, 0x4FU, 0xA5U, 0x59U, 0xBFU, 0x10U,
                        0x6AU, 0xAEU, 0xB5U, 0x28U, 0x6CU, 0x94U, 0xBAU, 0x34U,
                        0x18U, 0xFDU, 0xF3U, 0x53U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x70U && addr[ 2] == 0x39U && addr[ 3] == 0x52U &&
                        addr[ 4] == 0x79U && addr[ 5] == 0x73U && addr[ 6] == 0x42U && addr[ 7] == 0x63U &&
                        addr[ 8] == 0x55U && addr[ 9] == 0x42U && addr[10] == 0x59U && addr[11] == 0x63U &&
                        addr[12] == 0x76U && addr[13] == 0x4AU && addr[14] == 0x4AU && addr[15] == 0x4BU &&
                        addr[16] == 0x38U && addr[17] == 0x54U && addr[18] == 0x48U && addr[19] == 0x45U &&
                        addr[20] == 0x79U && addr[21] == 0x6FU && addr[22] == 0x79U && addr[23] == 0x74U &&
                        addr[24] == 0x74U && addr[25] == 0x6BU && addr[26] == 0x57U && addr[27] == 0x58U &&
                        addr[28] == 0x39U && addr[29] == 0x4BU && addr[30] == 0x52U && addr[31] == 0x62U &&
                        addr[32] == 0x39U && addr[33] == 0x4DU);
                }
                {
                    uint8_t raw[20] = {
                        0x75U, 0x82U, 0xFBU, 0x27U, 0x10U, 0x8CU, 0x0FU, 0x9AU,
                        0xF2U, 0x67U, 0x35U, 0xCCU, 0x7BU, 0x22U, 0x6BU, 0xD2U,
                        0x2FU, 0xDFU, 0x4FU, 0x92U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x42U && addr[ 2] == 0x35U && addr[ 3] == 0x4CU &&
                        addr[ 4] == 0x79U && addr[ 5] == 0x77U && addr[ 6] == 0x6BU && addr[ 7] == 0x54U &&
                        addr[ 8] == 0x4CU && addr[ 9] == 0x31U && addr[10] == 0x34U && addr[11] == 0x51U &&
                        addr[12] == 0x64U && addr[13] == 0x55U && addr[14] == 0x64U && addr[15] == 0x77U &&
                        addr[16] == 0x43U && addr[17] == 0x78U && addr[18] == 0x70U && addr[19] == 0x6EU &&
                        addr[20] == 0x65U && addr[21] == 0x46U && addr[22] == 0x32U && addr[23] == 0x7AU &&
                        addr[24] == 0x63U && addr[25] == 0x7AU && addr[26] == 0x46U && addr[27] == 0x66U &&
                        addr[28] == 0x44U && addr[29] == 0x7AU && addr[30] == 0x57U && addr[31] == 0x46U &&
                        addr[32] == 0x38U && addr[33] == 0x50U);
                }
                {
                    uint8_t raw[20] = {
                        0x6CU, 0xB6U, 0x51U, 0x1FU, 0x20U, 0xECU, 0xCAU, 0x1EU,
                        0x98U, 0x03U, 0xFCU, 0xFAU, 0x6FU, 0x3EU, 0x56U, 0x75U,
                        0x72U, 0x29U, 0x51U, 0x97U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x77U && addr[ 2] == 0x75U && addr[ 3] == 0x46U &&
                        addr[ 4] == 0x50U && addr[ 5] == 0x4BU && addr[ 6] == 0x34U && addr[ 7] == 0x48U &&
                        addr[ 8] == 0x51U && addr[ 9] == 0x4EU && addr[10] == 0x73U && addr[11] == 0x59U &&
                        addr[12] == 0x42U && addr[13] == 0x47U && addr[14] == 0x74U && addr[15] == 0x46U &&
                        addr[16] == 0x52U && addr[17] == 0x4BU && addr[18] == 0x77U && addr[19] == 0x45U &&
                        addr[20] == 0x6DU && addr[21] == 0x75U && addr[22] == 0x41U && addr[23] == 0x68U &&
                        addr[24] == 0x63U && addr[25] == 0x4BU && addr[26] == 0x63U && addr[27] == 0x48U &&
                        addr[28] == 0x39U && addr[29] == 0x5AU && addr[30] == 0x32U && addr[31] == 0x59U &&
                        addr[32] == 0x7AU && addr[33] == 0x58U);
                }
                {
                    uint8_t raw[20] = {
                        0xA5U, 0x31U, 0x30U, 0x28U, 0xF9U, 0x62U, 0xE4U, 0x80U,
                        0x48U, 0x94U, 0x3BU, 0x1AU, 0x59U, 0xBBU, 0x5EU, 0x36U,
                        0x96U, 0xB3U, 0x44U, 0x35U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x47U && addr[ 2] == 0x68U && addr[ 3] == 0x54U &&
                        addr[ 4] == 0x52U && addr[ 5] == 0x4AU && addr[ 6] == 0x5AU && addr[ 7] == 0x31U &&
                        addr[ 8] == 0x56U && addr[ 9] == 0x4DU && addr[10] == 0x51U && addr[11] == 0x74U &&
                        addr[12] == 0x36U && addr[13] == 0x6AU && addr[14] == 0x44U && addr[15] == 0x72U &&
                        addr[16] == 0x66U && addr[17] == 0x4EU && addr[18] == 0x63U && addr[19] == 0x6FU &&
                        addr[20] == 0x4AU && addr[21] == 0x34U && addr[22] == 0x39U && addr[23] == 0x6AU &&
                        addr[24] == 0x34U && addr[25] == 0x43U && addr[26] == 0x67U && addr[27] == 0x71U &&
                        addr[28] == 0x4BU && addr[29] == 0x6DU && addr[30] == 0x52U && addr[31] == 0x32U &&
                        addr[32] == 0x6FU && addr[33] == 0x36U);
                }
                {
                    uint8_t raw[20] = {
                        0xBFU, 0x04U, 0x6CU, 0x79U, 0xA0U, 0x96U, 0xDEU, 0x80U,
                        0x66U, 0xD3U, 0x74U, 0xC8U, 0xDFU, 0x94U, 0x5FU, 0x89U,
                        0xF2U, 0x3EU, 0x9AU, 0x27U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x4AU && addr[ 2] == 0x52U && addr[ 3] == 0x72U &&
                        addr[ 4] == 0x34U && addr[ 5] == 0x72U && addr[ 6] == 0x4CU && addr[ 7] == 0x32U &&
                        addr[ 8] == 0x43U && addr[ 9] == 0x4AU && addr[10] == 0x48U && addr[11] == 0x67U &&
                        addr[12] == 0x46U && addr[13] == 0x47U && addr[14] == 0x56U && addr[15] == 0x67U &&
                        addr[16] == 0x31U && addr[17] == 0x6AU && addr[18] == 0x61U && addr[19] == 0x66U &&
                        addr[20] == 0x39U && addr[21] == 0x4AU && addr[22] == 0x48U && addr[23] == 0x51U &&
                        addr[24] == 0x70U && addr[25] == 0x56U && addr[26] == 0x6DU && addr[27] == 0x68U &&
                        addr[28] == 0x76U && addr[29] == 0x45U && addr[30] == 0x37U && addr[31] == 0x68U &&
                        addr[32] == 0x61U && addr[33] == 0x62U);
                }
                {
                    uint8_t raw[20] = {
                        0xE2U, 0x07U, 0xABU, 0xD3U, 0x7DU, 0xC2U, 0xCDU, 0xD4U,
                        0x6DU, 0x15U, 0x7BU, 0x67U, 0x5AU, 0xC8U, 0x3EU, 0x0EU,
                        0x05U, 0x9BU, 0x08U, 0x62U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x4DU && addr[ 2] == 0x63U && addr[ 3] == 0x33U &&
                        addr[ 4] == 0x75U && addr[ 5] == 0x4DU && addr[ 6] == 0x6BU && addr[ 7] == 0x4BU &&
                        addr[ 8] == 0x31U && addr[ 9] == 0x62U && addr[10] == 0x62U && addr[11] == 0x32U &&
                        addr[12] == 0x64U && addr[13] == 0x4BU && addr[14] == 0x7AU && addr[15] == 0x5AU &&
                        addr[16] == 0x64U && addr[17] == 0x56U && addr[18] == 0x71U && addr[19] == 0x35U &&
                        addr[20] == 0x75U && addr[21] == 0x59U && addr[22] == 0x54U && addr[23] == 0x55U &&
                        addr[24] == 0x37U && addr[25] == 0x5AU && addr[26] == 0x76U && addr[27] == 0x4EU &&
                        addr[28] == 0x45U && addr[29] == 0x41U && addr[30] == 0x32U && addr[31] == 0x33U &&
                        addr[32] == 0x67U && addr[33] == 0x44U);
                }
                {
                    uint8_t raw[20] = {
                        0x2AU, 0x56U, 0x74U, 0x25U, 0x84U, 0x8DU, 0x41U, 0x6DU,
                        0xF1U, 0x06U, 0x01U, 0x6CU, 0x2AU, 0xB1U, 0x13U, 0xC3U,
                        0x1EU, 0x65U, 0x63U, 0x80U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x68U && addr[ 2] == 0x69U && addr[ 3] == 0x69U &&
                        addr[ 4] == 0x41U && addr[ 5] == 0x78U && addr[ 6] == 0x79U && addr[ 7] == 0x59U &&
                        addr[ 8] == 0x41U && addr[ 9] == 0x43U && addr[10] == 0x67U && addr[11] == 0x45U &&
                        addr[12] == 0x52U && addr[13] == 0x4BU && addr[14] == 0x47U && addr[15] == 0x51U &&
                        addr[16] == 0x4DU && addr[17] == 0x72U && addr[18] == 0x53U && addr[19] == 0x5AU &&
                        addr[20] == 0x57U && addr[21] == 0x43U && addr[22] == 0x74U && addr[23] == 0x6BU &&
                        addr[24] == 0x4DU && addr[25] == 0x6FU && addr[26] == 0x69U && addr[27] == 0x58U &&
                        addr[28] == 0x48U && addr[29] == 0x34U && addr[30] == 0x64U && addr[31] == 0x48U &&
                        addr[32] == 0x6EU && addr[33] == 0x6FU);
                }
                {
                    uint8_t raw[20] = {
                        0x24U, 0xBBU, 0xA9U, 0xC3U, 0x95U, 0x74U, 0x9AU, 0x88U,
                        0x04U, 0x12U, 0xC0U, 0x91U, 0xE7U, 0x13U, 0x41U, 0x7FU,
                        0x9AU, 0xD5U, 0x74U, 0x43U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x68U && addr[ 2] == 0x4DU && addr[ 3] == 0x4EU &&
                        addr[ 4] == 0x33U && addr[ 5] == 0x79U && addr[ 6] == 0x4EU && addr[ 7] == 0x50U &&
                        addr[ 8] == 0x4EU && addr[ 9] == 0x74U && addr[10] == 0x4BU && addr[11] == 0x70U &&
                        addr[12] == 0x78U && addr[13] == 0x6BU && addr[14] == 0x71U && addr[15] == 0x4CU &&
                        addr[16] == 0x78U && addr[17] == 0x51U && addr[18] == 0x32U && addr[19] == 0x63U &&
                        addr[20] == 0x33U && addr[21] == 0x55U && addr[22] == 0x68U && addr[23] == 0x6FU &&
                        addr[24] == 0x41U && addr[25] == 0x7AU && addr[26] == 0x66U && addr[27] == 0x75U &&
                        addr[28] == 0x59U && addr[29] == 0x35U && addr[30] == 0x75U && addr[31] == 0x35U &&
                        addr[32] == 0x4AU && addr[33] == 0x7AU);
                }
                {
                    uint8_t raw[20] = {
                        0x49U, 0x53U, 0x9EU, 0x65U, 0x21U, 0x8AU, 0xCFU, 0x37U,
                        0x85U, 0x2BU, 0xFFU, 0x87U, 0x14U, 0x76U, 0xDAU, 0x1AU,
                        0x62U, 0x3AU, 0xEAU, 0x80U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x66U && addr[ 2] == 0x67U && addr[ 3] == 0x35U &&
                        addr[ 4] == 0x56U && addr[ 5] == 0x41U && addr[ 6] == 0x44U && addr[ 7] == 0x41U &&
                        addr[ 8] == 0x4DU && addr[ 9] == 0x4DU && addr[10] == 0x42U && addr[11] == 0x78U &&
                        addr[12] == 0x46U && addr[13] == 0x51U && addr[14] == 0x76U && addr[15] == 0x44U &&
                        addr[16] == 0x78U && addr[17] == 0x5AU && addr[18] == 0x54U && addr[19] == 0x32U &&
                        addr[20] == 0x52U && addr[21] == 0x6AU && addr[22] == 0x55U && addr[23] == 0x64U &&
                        addr[24] == 0x47U && addr[25] == 0x69U && addr[26] == 0x64U && addr[27] == 0x59U &&
                        addr[28] == 0x61U && addr[29] == 0x35U && addr[30] == 0x76U && addr[31] == 0x69U &&
                        addr[32] == 0x37U && addr[33] == 0x5AU);
                }
                {
                    uint8_t raw[20] = {
                        0xE7U, 0xD3U, 0x03U, 0xBCU, 0xAEU, 0xBDU, 0x62U, 0x20U,
                        0xAEU, 0xC2U, 0xE1U, 0x7EU, 0x0BU, 0xFFU, 0xDCU, 0x21U,
                        0x24U, 0x34U, 0x50U, 0x82U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x34U && addr[ 2] == 0x33U && addr[ 3] == 0x6DU &&
                        addr[ 4] == 0x31U && addr[ 5] == 0x31U && addr[ 6] == 0x66U && addr[ 7] == 0x74U &&
                        addr[ 8] == 0x36U && addr[ 9] == 0x79U && addr[10] == 0x6FU && addr[11] == 0x50U &&
                        addr[12] == 0x69U && addr[13] == 0x6DU && addr[14] == 0x36U && addr[15] == 0x56U &&
                        addr[16] == 0x44U && addr[17] == 0x78U && addr[18] == 0x64U && addr[19] == 0x55U &&
                        addr[20] == 0x76U && addr[21] == 0x63U && addr[22] == 0x46U && addr[23] == 0x77U &&
                        addr[24] == 0x36U && addr[25] == 0x57U && addr[26] == 0x38U && addr[27] == 0x41U &&
                        addr[28] == 0x77U && addr[29] == 0x78U && addr[30] == 0x78U && addr[31] == 0x4BU &&
                        addr[32] == 0x35U && addr[33] == 0x58U);
                }
                {
                    uint8_t raw[20] = {
                        0xC3U, 0xE1U, 0x5FU, 0xABU, 0xC0U, 0x0AU, 0x79U, 0x73U,
                        0x71U, 0xD0U, 0x55U, 0xC0U, 0x80U, 0x79U, 0xAEU, 0x45U,
                        0x71U, 0x0FU, 0xA0U, 0x97U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x4AU && addr[ 2] == 0x69U && addr[ 3] == 0x35U &&
                        addr[ 4] == 0x6BU && addr[ 5] == 0x65U && addr[ 6] == 0x58U && addr[ 7] == 0x79U &&
                        addr[ 8] == 0x33U && addr[ 9] == 0x31U && addr[10] == 0x4AU && addr[11] == 0x31U &&
                        addr[12] == 0x34U && addr[13] == 0x52U && addr[14] == 0x4BU && addr[15] == 0x73U &&
                        addr[16] == 0x4EU && addr[17] == 0x59U && addr[18] == 0x41U && addr[19] == 0x46U &&
                        addr[20] == 0x31U && addr[21] == 0x51U && addr[22] == 0x36U && addr[23] == 0x6AU &&
                        addr[24] == 0x4DU && addr[25] == 0x56U && addr[26] == 0x69U && addr[27] == 0x45U &&
                        addr[28] == 0x52U && addr[29] == 0x55U && addr[30] == 0x51U && addr[31] == 0x71U &&
                        addr[32] == 0x59U && addr[33] == 0x36U);
                }
                {
                    uint8_t raw[20] = {
                        0x95U, 0x15U, 0x7FU, 0x2AU, 0xAFU, 0xE3U, 0x2FU, 0x7FU,
                        0x2EU, 0xF1U, 0xA0U, 0xF5U, 0xEAU, 0xC3U, 0x07U, 0x06U,
                        0xA1U, 0xD3U, 0xF5U, 0xD9U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x4EU && addr[ 2] == 0x62U && addr[ 3] == 0x48U &&
                        addr[ 4] == 0x53U && addr[ 5] == 0x55U && addr[ 6] == 0x6DU && addr[ 7] == 0x66U &&
                        addr[ 8] == 0x4BU && addr[ 9] == 0x61U && addr[10] == 0x34U && addr[11] == 0x71U &&
                        addr[12] == 0x31U && addr[13] == 0x51U && addr[14] == 0x78U && addr[15] == 0x44U &&
                        addr[16] == 0x45U && addr[17] == 0x5AU && addr[18] == 0x4CU && addr[19] == 0x6EU &&
                        addr[20] == 0x54U && addr[21] == 0x67U && addr[22] == 0x46U && addr[23] == 0x56U &&
                        addr[24] == 0x45U && addr[25] == 0x4CU && addr[26] == 0x78U && addr[27] == 0x39U &&
                        addr[28] == 0x6DU && addr[29] == 0x57U && addr[30] == 0x45U && addr[31] == 0x43U &&
                        addr[32] == 0x6BU && addr[33] == 0x41U);
                }
                {
                    uint8_t raw[20] = {
                        0xF0U, 0xECU, 0x0FU, 0x86U, 0x31U, 0xBBU, 0x2CU, 0xBFU,
                        0x8FU, 0xB7U, 0xE3U, 0x1CU, 0x82U, 0xA0U, 0xA3U, 0x50U,
                        0xD5U, 0xE0U, 0xFEU, 0x6BU
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x34U && addr[ 2] == 0x78U && addr[ 3] == 0x31U &&
                        addr[ 4] == 0x78U && addr[ 5] == 0x46U && addr[ 6] == 0x32U && addr[ 7] == 0x42U &&
                        addr[ 8] == 0x47U && addr[ 9] == 0x73U && addr[10] == 0x42U && addr[11] == 0x41U &&
                        addr[12] == 0x7AU && addr[13] == 0x77U && addr[14] == 0x77U && addr[15] == 0x61U &&
                        addr[16] == 0x4BU && addr[17] == 0x61U && addr[18] == 0x70U && addr[19] == 0x4BU &&
                        addr[20] == 0x6FU && addr[21] == 0x6FU && addr[22] == 0x35U && addr[23] == 0x57U &&
                        addr[24] == 0x65U && addr[25] == 0x31U && addr[26] == 0x59U && addr[27] == 0x53U &&
                        addr[28] == 0x6EU && addr[29] == 0x52U && addr[30] == 0x50U && addr[31] == 0x57U &&
                        addr[32] == 0x75U && addr[33] == 0x39U);
                }
                {
                    uint8_t raw[20] = {
                        0x8DU, 0xA4U, 0x7DU, 0xABU, 0xD1U, 0x19U, 0xDCU, 0xC4U,
                        0x45U, 0x5FU, 0xAAU, 0xE2U, 0x1CU, 0x39U, 0xCAU, 0x19U,
                        0x34U, 0xF1U, 0x86U, 0x16U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x44U && addr[ 2] == 0x75U && addr[ 3] == 0x41U &&
                        addr[ 4] == 0x4CU && addr[ 5] == 0x6EU && addr[ 6] == 0x52U && addr[ 7] == 0x79U &&
                        addr[ 8] == 0x76U && addr[ 9] == 0x77U && addr[10] == 0x38U && addr[11] == 0x36U &&
                        addr[12] == 0x43U && addr[13] == 0x63U && addr[14] == 0x55U && addr[15] == 0x5AU &&
                        addr[16] == 0x39U && addr[17] == 0x74U && addr[18] == 0x52U && addr[19] == 0x55U &&
                        addr[20] == 0x45U && addr[21] == 0x6DU && addr[22] == 0x35U && addr[23] == 0x43U &&
                        addr[24] == 0x61U && addr[25] == 0x65U && addr[26] == 0x50U && addr[27] == 0x46U &&
                        addr[28] == 0x66U && addr[29] == 0x33U && addr[30] == 0x74U && addr[31] == 0x36U &&
                        addr[32] == 0x61U && addr[33] == 0x31U);
                }
                {
                    uint8_t raw[20] = {
                        0xA9U, 0x94U, 0x5AU, 0xE3U, 0x5AU, 0x43U, 0xADU, 0xBEU,
                        0xBAU, 0xA4U, 0x13U, 0x94U, 0xF5U, 0xDCU, 0x8FU, 0x3BU,
                        0x01U, 0x14U, 0xFFU, 0xFEU
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x47U && addr[ 2] == 0x54U && addr[ 3] == 0x65U &&
                        addr[ 4] == 0x76U && addr[ 5] == 0x4AU && addr[ 6] == 0x71U && addr[ 7] == 0x76U &&
                        addr[ 8] == 0x6BU && addr[ 9] == 0x5AU && addr[10] == 0x76U && addr[11] == 0x48U &&
                        addr[12] == 0x73U && addr[13] == 0x58U && addr[14] == 0x5AU && addr[15] == 0x71U &&
                        addr[16] == 0x55U && addr[17] == 0x78U && addr[18] == 0x43U && addr[19] == 0x48U &&
                        addr[20] == 0x4CU && addr[21] == 0x68U && addr[22] == 0x73U && addr[23] == 0x43U &&
                        addr[24] == 0x53U && addr[25] == 0x38U && addr[26] == 0x57U && addr[27] == 0x68U &&
                        addr[28] == 0x79U && addr[29] == 0x4DU && addr[30] == 0x74U && addr[31] == 0x7AU &&
                        addr[32] == 0x6EU && addr[33] == 0x5AU);
                }
                {
                    uint8_t raw[20] = {
                        0xC1U, 0xE6U, 0x7FU, 0x17U, 0xD3U, 0x00U, 0x9BU, 0x80U,
                        0x6CU, 0x85U, 0x74U, 0x9CU, 0x80U, 0x40U, 0xAFU, 0x64U,
                        0xCEU, 0x09U, 0x7EU, 0x2EU
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x4AU && addr[ 2] == 0x67U && addr[ 3] == 0x45U &&
                        addr[ 4] == 0x59U && addr[ 5] == 0x55U && addr[ 6] == 0x37U && addr[ 7] == 0x36U &&
                        addr[ 8] == 0x45U && addr[ 9] == 0x55U && addr[10] == 0x34U && addr[11] == 0x59U &&
                        addr[12] == 0x41U && addr[13] == 0x5AU && addr[14] == 0x41U && addr[15] == 0x44U &&
                        addr[16] == 0x79U && addr[17] == 0x61U && addr[18] == 0x37U && addr[19] == 0x6BU &&
                        addr[20] == 0x37U && addr[21] == 0x62U && addr[22] == 0x71U && addr[23] == 0x38U &&
                        addr[24] == 0x4EU && addr[25] == 0x76U && addr[26] == 0x64U && addr[27] == 0x65U &&
                        addr[28] == 0x4BU && addr[29] == 0x41U && addr[30] == 0x48U && addr[31] == 0x69U &&
                        addr[32] == 0x32U && addr[33] == 0x50U);
                }
                {
                    uint8_t raw[20] = {
                        0xD8U, 0x74U, 0xCFU, 0x61U, 0x0DU, 0x97U, 0xE4U, 0xABU,
                        0x76U, 0xA0U, 0x70U, 0x60U, 0xB7U, 0xC5U, 0x9CU, 0x9AU,
                        0x88U, 0x86U, 0x62U, 0xAAU
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x4CU && addr[ 2] == 0x6AU && addr[ 3] == 0x57U &&
                        addr[ 4] == 0x74U && addr[ 5] == 0x59U && addr[ 6] == 0x52U && addr[ 7] == 0x61U &&
                        addr[ 8] == 0x6EU && addr[ 9] == 0x61U && addr[10] == 0x6BU && addr[11] == 0x33U &&
                        addr[12] == 0x74U && addr[13] == 0x52U && addr[14] == 0x43U && addr[15] == 0x5AU &&
                        addr[16] == 0x42U && addr[17] == 0x69U && addr[18] == 0x61U && addr[19] == 0x38U &&
                        addr[20] == 0x64U && addr[21] == 0x33U && addr[22] == 0x70U && addr[23] == 0x7AU &&
                        addr[24] == 0x78U && addr[25] == 0x6BU && addr[26] == 0x6EU && addr[27] == 0x63U &&
                        addr[28] == 0x73U && addr[29] == 0x7AU && addr[30] == 0x6FU && addr[31] == 0x33U &&
                        addr[32] == 0x33U && addr[33] == 0x38U);
                }
                {
                    uint8_t raw[20] = {
                        0x8EU, 0xADU, 0xB4U, 0xBBU, 0x71U, 0x2AU, 0x29U, 0x1BU,
                        0x53U, 0x43U, 0xE0U, 0x03U, 0x1FU, 0x97U, 0x6BU, 0x0DU,
                        0xA9U, 0xEDU, 0x39U, 0xC2U
                    };
                    uint8_t addr[50];
                    ASSERT(34 == 
                        util_raddr((uint32_t)addr, sizeof(addr), raw, 20));
                    ASSERT(
                        addr[ 0] == 0x72U && addr[ 1] == 0x4EU && addr[ 2] == 0x72U && addr[ 3] == 0x52U &&
                        addr[ 4] == 0x73U && addr[ 5] == 0x59U && addr[ 6] == 0x57U && addr[ 7] == 0x69U &&
                        addr[ 8] == 0x4AU && addr[ 9] == 0x53U && addr[10] == 0x64U && addr[11] == 0x39U &&
                        addr[12] == 0x47U && addr[13] == 0x4AU && addr[14] == 0x50U && addr[15] == 0x50U &&
                        addr[16] == 0x36U && addr[17] == 0x51U && addr[18] == 0x71U && addr[19] == 0x33U &&
                        addr[20] == 0x4AU && addr[21] == 0x61U && addr[22] == 0x44U && addr[23] == 0x43U &&
                        addr[24] == 0x37U && addr[25] == 0x53U && addr[26] == 0x48U && addr[27] == 0x61U &&
                        addr[28] == 0x57U && addr[29] == 0x66U && addr[30] == 0x68U && addr[31] == 0x32U &&
                        addr[32] == 0x33U && addr[33] == 0x4BU);
                }                

                // Test out of bounds check
                ASSERT(util_raddr(1000000, 50, 0, 20) == OUT_OF_BOUNDS);
                ASSERT(util_raddr(0, 50, 10000000, 20) == OUT_OF_BOUNDS);
                uint8_t raw[20] = {
                    0x8EU, 0xADU, 0xB4U, 0xBBU, 0x71U, 0x2AU, 0x29U, 0x1BU,
                    0x53U, 0x43U, 0xE0U, 0x03U, 0x1FU, 0x97U, 0x6BU, 0x0DU,
                    0xA9U, 0xEDU, 0x39U, 0xC2U
                };
                ASSERT(util_raddr(0, 30, raw, 20) == TOO_SMALL);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set util_raddr"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test util_raddr"), fee(XRP(1)));
    }

    void
    test_util_sha512h()
    {
        testcase("Test util_sha512h");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t util_sha512h (uint32_t, uint32_t, uint32_t, uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);
            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                {
                    uint8_t raw[20] = {
                        0x72U, 0x4EU, 0x36U, 0x53U, 0x59U, 0x77U, 0x72U, 0x32U,
                        0x64U, 0x54U, 0x56U, 0x43U, 0x7AU, 0x45U, 0x71U, 0x39U,
                        0x57U, 0x43U, 0x77U, 0x4AU
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x42U && hash[ 1] == 0x5CU && hash[ 2] == 0x4CU && hash[ 3] == 0x01U &&
                        hash[ 4] == 0x84U && hash[ 5] == 0xA5U && hash[ 6] == 0x76U && hash[ 7] == 0x79U &&
                        hash[ 8] == 0xDCU && hash[ 9] == 0x6DU && hash[10] == 0xFFU && hash[11] == 0x40U &&
                        hash[12] == 0x8CU && hash[13] == 0x29U && hash[14] == 0x06U && hash[15] == 0x6BU &&
                        hash[16] == 0x0FU && hash[17] == 0xB9U && hash[18] == 0xEAU && hash[19] == 0x34U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x4BU, 0x4BU, 0x75U, 0x52U, 0x36U, 0x36U, 0x46U,
                        0x62U, 0x38U, 0x33U, 0x76U, 0x35U, 0x71U, 0x79U, 0x41U,
                        0x34U, 0x48U, 0x67U, 0x6AU
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x36U && hash[ 1] == 0x2CU && hash[ 2] == 0x32U && hash[ 3] == 0x1DU &&
                        hash[ 4] == 0x8DU && hash[ 5] == 0xDDU && hash[ 6] == 0xAFU && hash[ 7] == 0x2DU &&
                        hash[ 8] == 0x3CU && hash[ 9] == 0xE6U && hash[10] == 0x94U && hash[11] == 0x12U &&
                        hash[12] == 0x20U && hash[13] == 0xDAU && hash[14] == 0x62U && hash[15] == 0xA6U &&
                        hash[16] == 0x98U && hash[17] == 0x41U && hash[18] == 0x04U && hash[19] == 0x5EU);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x42U, 0x54U, 0x33U, 0x58U, 0x57U, 0x43U, 0x76U,
                        0x61U, 0x38U, 0x48U, 0x55U, 0x4EU, 0x4EU, 0x5AU, 0x46U,
                        0x6AU, 0x5AU, 0x43U, 0x55U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0xCFU && hash[ 1] == 0xFDU && hash[ 2] == 0x6FU && hash[ 3] == 0x01U &&
                        hash[ 4] == 0x95U && hash[ 5] == 0x76U && hash[ 6] == 0x7DU && hash[ 7] == 0xFBU &&
                        hash[ 8] == 0xCAU && hash[ 9] == 0x41U && hash[10] == 0xFDU && hash[11] == 0x24U &&
                        hash[12] == 0x23U && hash[13] == 0xD6U && hash[14] == 0x82U && hash[15] == 0x20U &&
                        hash[16] == 0x76U && hash[17] == 0xDDU && hash[18] == 0xC9U && hash[19] == 0xECU);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x4CU, 0x52U, 0x4CU, 0x41U, 0x6EU, 0x61U, 0x62U,
                        0x56U, 0x6FU, 0x46U, 0x62U, 0x37U, 0x47U, 0x68U, 0x79U,
                        0x58U, 0x75U, 0x42U, 0x53U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x02U && hash[ 1] == 0xEBU && hash[ 2] == 0x2FU && hash[ 3] == 0x30U &&
                        hash[ 4] == 0xFCU && hash[ 5] == 0x73U && hash[ 6] == 0x34U && hash[ 7] == 0xE7U &&
                        hash[ 8] == 0x89U && hash[ 9] == 0xA2U && hash[10] == 0x58U && hash[11] == 0xD6U &&
                        hash[12] == 0xB0U && hash[13] == 0x55U && hash[14] == 0x32U && hash[15] == 0x96U &&
                        hash[16] == 0xB5U && hash[17] == 0x2EU && hash[18] == 0x97U && hash[19] == 0x81U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x4CU, 0x37U, 0x33U, 0x39U, 0x47U, 0x4BU, 0x35U,
                        0x75U, 0x36U, 0x79U, 0x78U, 0x76U, 0x43U, 0x73U, 0x6FU,
                        0x68U, 0x43U, 0x32U, 0x43U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x9FU && hash[ 1] == 0xD4U && hash[ 2] == 0x7CU && hash[ 3] == 0x25U &&
                        hash[ 4] == 0xDEU && hash[ 5] == 0x23U && hash[ 6] == 0x97U && hash[ 7] == 0x57U &&
                        hash[ 8] == 0xEDU && hash[ 9] == 0x25U && hash[10] == 0xD0U && hash[11] == 0x98U &&
                        hash[12] == 0xF7U && hash[13] == 0x83U && hash[14] == 0x70U && hash[15] == 0xF6U &&
                        hash[16] == 0x5FU && hash[17] == 0x3DU && hash[18] == 0xB5U && hash[19] == 0x43U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x4DU, 0x4DU, 0x45U, 0x57U, 0x74U, 0x75U, 0x4BU,
                        0x43U, 0x77U, 0x54U, 0x43U, 0x36U, 0x31U, 0x78U, 0x41U,
                        0x78U, 0x35U, 0x55U, 0x46U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x77U && hash[ 1] == 0x59U && hash[ 2] == 0x43U && hash[ 3] == 0x6BU &&
                        hash[ 4] == 0x4DU && hash[ 5] == 0x11U && hash[ 6] == 0x6BU && hash[ 7] == 0xE5U &&
                        hash[ 8] == 0xF8U && hash[ 9] == 0x90U && hash[10] == 0x07U && hash[11] == 0x00U &&
                        hash[12] == 0xB3U && hash[13] == 0xB2U && hash[14] == 0x6BU && hash[15] == 0x8AU &&
                        hash[16] == 0xC8U && hash[17] == 0xF2U && hash[18] == 0x82U && hash[19] == 0xB7U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x66U, 0x48U, 0x6AU, 0x66U, 0x31U, 0x6BU, 0x70U,
                        0x4BU, 0x6AU, 0x39U, 0x66U, 0x6AU, 0x39U, 0x35U, 0x58U,
                        0x6AU, 0x59U, 0x69U, 0x51U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0xBDU && hash[ 1] == 0x1BU && hash[ 2] == 0xDDU && hash[ 3] == 0x9DU &&
                        hash[ 4] == 0x10U && hash[ 5] == 0xDEU && hash[ 6] == 0x24U && hash[ 7] == 0xA1U &&
                        hash[ 8] == 0xB2U && hash[ 9] == 0x6CU && hash[10] == 0x24U && hash[11] == 0xBCU &&
                        hash[12] == 0xF9U && hash[13] == 0x97U && hash[14] == 0x50U && hash[15] == 0xDEU &&
                        hash[16] == 0x93U && hash[17] == 0x39U && hash[18] == 0x58U && hash[19] == 0x21U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x66U, 0x6EU, 0x75U, 0x57U, 0x38U, 0x77U, 0x6FU,
                        0x4BU, 0x62U, 0x6EU, 0x57U, 0x4BU, 0x6BU, 0x6BU, 0x75U,
                        0x39U, 0x6AU, 0x79U, 0x64U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x3BU && hash[ 1] == 0x89U && hash[ 2] == 0xEDU && hash[ 3] == 0x68U &&
                        hash[ 4] == 0x0DU && hash[ 5] == 0x13U && hash[ 6] == 0x3BU && hash[ 7] == 0x1DU &&
                        hash[ 8] == 0x43U && hash[ 9] == 0xFEU && hash[10] == 0xAEU && hash[11] == 0x3EU &&
                        hash[12] == 0xC3U && hash[13] == 0x90U && hash[14] == 0xE8U && hash[15] == 0x0EU &&
                        hash[16] == 0x17U && hash[17] == 0x14U && hash[18] == 0x23U && hash[19] == 0x71U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x70U, 0x79U, 0x64U, 0x52U, 0x39U, 0x55U, 0x32U,
                        0x67U, 0x66U, 0x75U, 0x6BU, 0x34U, 0x5AU, 0x72U, 0x53U,
                        0x66U, 0x48U, 0x61U, 0x71U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x2BU && hash[ 1] == 0x01U && hash[ 2] == 0x00U && hash[ 3] == 0x05U &&
                        hash[ 4] == 0xF1U && hash[ 5] == 0x60U && hash[ 6] == 0x71U && hash[ 7] == 0x62U &&
                        hash[ 8] == 0x7CU && hash[ 9] == 0x4AU && hash[10] == 0xCCU && hash[11] == 0x03U &&
                        hash[12] == 0x2AU && hash[13] == 0x89U && hash[14] == 0x40U && hash[15] == 0x5AU &&
                        hash[16] == 0x03U && hash[17] == 0xDCU && hash[18] == 0x83U && hash[19] == 0xC8U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x4CU, 0x4CU, 0x45U, 0x36U, 0x34U, 0x74U, 0x44U,
                        0x4CU, 0x78U, 0x59U, 0x37U, 0x47U, 0x6FU, 0x41U, 0x41U,
                        0x57U, 0x66U, 0x73U, 0x36U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0xDFU && hash[ 1] == 0xE3U && hash[ 2] == 0x14U && hash[ 3] == 0xF0U &&
                        hash[ 4] == 0x5FU && hash[ 5] == 0x95U && hash[ 6] == 0x8CU && hash[ 7] == 0x57U &&
                        hash[ 8] == 0x2FU && hash[ 9] == 0x9DU && hash[10] == 0x45U && hash[11] == 0xDCU &&
                        hash[12] == 0x12U && hash[13] == 0x77U && hash[14] == 0x39U && hash[15] == 0xACU &&
                        hash[16] == 0xEAU && hash[17] == 0x4AU && hash[18] == 0xB0U && hash[19] == 0x8FU);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x50U, 0x64U, 0x50U, 0x58U, 0x77U, 0x76U, 0x75U,
                        0x39U, 0x4EU, 0x4CU, 0x59U, 0x46U, 0x50U, 0x34U, 0x69U,
                        0x56U, 0x64U, 0x56U, 0x70U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0xD1U && hash[ 1] == 0x9FU && hash[ 2] == 0x25U && hash[ 3] == 0x93U &&
                        hash[ 4] == 0xA3U && hash[ 5] == 0xCAU && hash[ 6] == 0xEAU && hash[ 7] == 0x10U &&
                        hash[ 8] == 0x06U && hash[ 9] == 0x78U && hash[10] == 0xFCU && hash[11] == 0x58U &&
                        hash[12] == 0xA4U && hash[13] == 0x99U && hash[14] == 0x3CU && hash[15] == 0x6EU &&
                        hash[16] == 0xC4U && hash[17] == 0x2DU && hash[18] == 0x6DU && hash[19] == 0x53U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x44U, 0x65U, 0x44U, 0x32U, 0x5AU, 0x71U, 0x53U,
                        0x48U, 0x35U, 0x44U, 0x70U, 0x51U, 0x4DU, 0x78U, 0x76U,
                        0x36U, 0x36U, 0x52U, 0x6BU
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x4DU && hash[ 1] == 0x5BU && hash[ 2] == 0xDDU && hash[ 3] == 0x31U &&
                        hash[ 4] == 0xDEU && hash[ 5] == 0xB9U && hash[ 6] == 0xF5U && hash[ 7] == 0xB8U &&
                        hash[ 8] == 0xBDU && hash[ 9] == 0x17U && hash[10] == 0xE1U && hash[11] == 0x51U &&
                        hash[12] == 0xAAU && hash[13] == 0x51U && hash[14] == 0x9CU && hash[15] == 0x5BU &&
                        hash[16] == 0xE0U && hash[17] == 0x15U && hash[18] == 0x61U && hash[19] == 0x2CU);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x55U, 0x34U, 0x78U, 0x54U, 0x52U, 0x75U, 0x6FU,
                        0x32U, 0x34U, 0x62U, 0x52U, 0x6FU, 0x65U, 0x41U, 0x48U,
                        0x33U, 0x53U, 0x55U, 0x66U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0xBDU && hash[ 1] == 0xA1U && hash[ 2] == 0x62U && hash[ 3] == 0x1EU &&
                        hash[ 4] == 0x84U && hash[ 5] == 0x12U && hash[ 6] == 0xB3U && hash[ 7] == 0xCCU &&
                        hash[ 8] == 0x58U && hash[ 9] == 0x19U && hash[10] == 0x9AU && hash[11] == 0x22U &&
                        hash[12] == 0xCFU && hash[13] == 0x6AU && hash[14] == 0x0AU && hash[15] == 0x43U &&
                        hash[16] == 0xDEU && hash[17] == 0xB5U && hash[18] == 0xBAU && hash[19] == 0x50U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x6EU, 0x6DU, 0x6FU, 0x6AU, 0x57U, 0x46U, 0x6FU,
                        0x41U, 0x58U, 0x72U, 0x76U, 0x71U, 0x75U, 0x62U, 0x6FU,
                        0x45U, 0x77U, 0x4EU, 0x4EU
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x5FU && hash[ 1] == 0x26U && hash[ 2] == 0xF9U && hash[ 3] == 0x0AU &&
                        hash[ 4] == 0xC7U && hash[ 5] == 0xD5U && hash[ 6] == 0x40U && hash[ 7] == 0x2DU &&
                        hash[ 8] == 0x1FU && hash[ 9] == 0x9EU && hash[10] == 0x46U && hash[11] == 0xAAU &&
                        hash[12] == 0x6DU && hash[13] == 0x9CU && hash[14] == 0x64U && hash[15] == 0x88U &&
                        hash[16] == 0x87U && hash[17] == 0xF3U && hash[18] == 0x29U && hash[19] == 0x72U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x61U, 0x33U, 0x57U, 0x65U, 0x64U, 0x69U, 0x71U,
                        0x58U, 0x37U, 0x34U, 0x79U, 0x42U, 0x42U, 0x68U, 0x48U,
                        0x4CU, 0x44U, 0x51U, 0x4DU
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x25U && hash[ 1] == 0x70U && hash[ 2] == 0x5FU && hash[ 3] == 0x6DU &&
                        hash[ 4] == 0xA8U && hash[ 5] == 0x60U && hash[ 6] == 0x54U && hash[ 7] == 0xBAU &&
                        hash[ 8] == 0xD8U && hash[ 9] == 0x33U && hash[10] == 0x41U && hash[11] == 0x48U &&
                        hash[12] == 0x95U && hash[13] == 0x52U && hash[14] == 0xA6U && hash[15] == 0x22U &&
                        hash[16] == 0x9DU && hash[17] == 0x82U && hash[18] == 0xA0U && hash[19] == 0x87U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x45U, 0x47U, 0x57U, 0x33U, 0x6BU, 0x6FU, 0x34U,
                        0x41U, 0x31U, 0x69U, 0x50U, 0x43U, 0x5AU, 0x54U, 0x78U,
                        0x6DU, 0x77U, 0x6AU, 0x44U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0xD4U && hash[ 1] == 0xDAU && hash[ 2] == 0xE0U && hash[ 3] == 0xC7U &&
                        hash[ 4] == 0x40U && hash[ 5] == 0xC4U && hash[ 6] == 0x28U && hash[ 7] == 0x59U &&
                        hash[ 8] == 0xA9U && hash[ 9] == 0x6DU && hash[10] == 0x91U && hash[11] == 0xDCU &&
                        hash[12] == 0x34U && hash[13] == 0x0DU && hash[14] == 0xB9U && hash[15] == 0xE6U &&
                        hash[16] == 0xE9U && hash[17] == 0x9DU && hash[18] == 0x04U && hash[19] == 0x0BU);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x68U, 0x52U, 0x46U, 0x71U, 0x54U, 0x35U, 0x45U,
                        0x39U, 0x7AU, 0x63U, 0x69U, 0x70U, 0x68U, 0x4CU, 0x54U,
                        0x39U, 0x78U, 0x6AU, 0x52U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x61U && hash[ 1] == 0x5BU && hash[ 2] == 0xFEU && hash[ 3] == 0x17U &&
                        hash[ 4] == 0x6EU && hash[ 5] == 0x81U && hash[ 6] == 0x42U && hash[ 7] == 0xFFU &&
                        hash[ 8] == 0xEEU && hash[ 9] == 0xD7U && hash[10] == 0x1AU && hash[11] == 0x6DU &&
                        hash[12] == 0x14U && hash[13] == 0x5DU && hash[14] == 0x64U && hash[15] == 0xA8U &&
                        hash[16] == 0x20U && hash[17] == 0x1AU && hash[18] == 0x33U && hash[19] == 0xC3U);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x70U, 0x61U, 0x4AU, 0x69U, 0x34U, 0x4CU, 0x62U,
                        0x55U, 0x36U, 0x55U, 0x63U, 0x4AU, 0x45U, 0x78U, 0x62U,
                        0x38U, 0x39U, 0x35U, 0x5AU
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x01U && hash[ 1] == 0x61U && hash[ 2] == 0xA4U && hash[ 3] == 0x8EU &&
                        hash[ 4] == 0x6DU && hash[ 5] == 0x20U && hash[ 6] == 0xBAU && hash[ 7] == 0x20U &&
                        hash[ 8] == 0x72U && hash[ 9] == 0x72U && hash[10] == 0x8FU && hash[11] == 0x4FU &&
                        hash[12] == 0x3FU && hash[13] == 0xE1U && hash[14] == 0xE1U && hash[15] == 0xE7U &&
                        hash[16] == 0xEBU && hash[17] == 0x15U && hash[18] == 0xA8U && hash[19] == 0x4CU);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x34U, 0x59U, 0x78U, 0x47U, 0x46U, 0x71U, 0x51U,
                        0x64U, 0x47U, 0x70U, 0x71U, 0x6EU, 0x4CU, 0x59U, 0x65U,
                        0x4DU, 0x38U, 0x56U, 0x52U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0x42U && hash[ 1] == 0xC5U && hash[ 2] == 0x2FU && hash[ 3] == 0x3BU &&
                        hash[ 4] == 0xB7U && hash[ 5] == 0xD4U && hash[ 6] == 0x54U && hash[ 7] == 0xB4U &&
                        hash[ 8] == 0x97U && hash[ 9] == 0xB4U && hash[10] == 0xFCU && hash[11] == 0xB0U &&
                        hash[12] == 0x46U && hash[13] == 0xBAU && hash[14] == 0xB6U && hash[15] == 0xADU &&
                        hash[16] == 0x93U && hash[17] == 0x8DU && hash[18] == 0xEBU && hash[19] == 0x7DU);
                }
                {
                    uint8_t raw[20] = {
                        0x72U, 0x33U, 0x76U, 0x71U, 0x75U, 0x79U, 0x72U, 0x45U,
                        0x39U, 0x55U, 0x53U, 0x70U, 0x68U, 0x62U, 0x43U, 0x55U,
                        0x6DU, 0x65U, 0x4BU, 0x55U
                    };
                    uint8_t hash[32];
                    ASSERT(32 == 
                        util_sha512h((uint32_t)hash, sizeof(hash), raw, 20));
                    ASSERT(
                        hash[ 0] == 0xD5U && hash[ 1] == 0x6BU && hash[ 2] == 0x6BU && hash[ 3] == 0x45U &&
                        hash[ 4] == 0x30U && hash[ 5] == 0xF0U && hash[ 6] == 0x34U && hash[ 7] == 0x76U &&
                        hash[ 8] == 0x31U && hash[ 9] == 0x56U && hash[10] == 0x8CU && hash[11] == 0x38U &&
                        hash[12] == 0x0CU && hash[13] == 0x1AU && hash[14] == 0xAFU && hash[15] == 0xABU &&
                        hash[16] == 0x42U && hash[17] == 0x16U && hash[18] == 0x21U && hash[19] == 0x42U);
                }

                // Test out of bounds check
                ASSERT(util_sha512h(1000000, 50, 0, 20) == OUT_OF_BOUNDS);
                ASSERT(util_sha512h(0, 50, 10000000, 20) == OUT_OF_BOUNDS);
                uint8_t raw[20] = {
                    0x8EU, 0xADU, 0xB4U, 0xBBU, 0x71U, 0x2AU, 0x29U, 0x1BU,
                    0x53U, 0x43U, 0xE0U, 0x03U, 0x1FU, 0x97U, 0x6BU, 0x0DU,
                    0xA9U, 0xEDU, 0x39U, 0xC2U
                };
                ASSERT(util_sha512h(0, 30, raw, 20) == TOO_SMALL);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set util_sha512h"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test util_sha512h"), fee(XRP(1)));
    }

    void
    test_util_verify()
    {
        testcase("Test util_verify");
        using namespace jtx;
        Env env{*this, supported_amendments()};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10000), alice);
        env.fund(XRP(10000), bob);

        TestHook hook = wasm[R"[test.hook](
            #include <stdint.h>
            extern int32_t _g       (uint32_t id, uint32_t maxiter);
            #define GUARD(maxiter) _g((1ULL << 31U) + __LINE__, (maxiter)+1)
            extern int64_t accept   (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t rollback (uint32_t read_ptr, uint32_t read_len, int64_t error_code);
            extern int64_t util_verify (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
            #define TOO_SMALL -4
            #define OUT_OF_BOUNDS -1
            #define INVALID_KEY -41
            #define SBUF(x) ((uint32_t)(x)), sizeof(x)
            #define ASSERT(x)\
                if (!(x))\
                    rollback((uint32_t)#x, sizeof(#x), __LINE__);

            // secp256k1
            uint8_t pubkey_sec[] =
            {
                0x02U,0xC7U,0x38U,0x7FU,0xFCU,0x25U,0xC1U,0x56U,0xCAU,0x7FU,
                0x8AU,0x6DU,0x76U,0x0CU,0x8DU,0x01U,0xEFU,0x64U,0x2CU,0xEEU,
                0x9CU,0xE4U,0x68U,0x0CU,0x33U,0xFFU,0xB3U,0xFFU,0x39U,0xAFU,
                0xECU,0xFEU,0x70U
            };

            uint8_t sig_sec[] =
            {
                0x30U,0x45U,0x02U,0x21U,0x00U,0x95U,0x6EU,0x7DU,0x1FU,0x01U,
                0x16U,0xF1U,0x65U,0x00U,0xD2U,0xCCU,0xD8U,0x8DU,0x2AU,0x2FU,
                0xEFU,0xF6U,0x52U,0x16U,0x85U,0x42U,0xF4U,0x4EU,0x43U,0xDBU,
                0xE6U,0xF4U,0x53U,0xE8U,0x03U,0xB8U,0x4FU,0x02U,0x20U,0x0AU,
                0xB6U,0xC3U,0x4BU,0x5FU,0x0CU,0xC6U,0x6BU,0x4FU,0x1FU,0x83U,
                0xE9U,0x89U,0x74U,0xB8U,0x80U,0xA2U,0x2FU,0xAEU,0x52U,0x91U,
                0x6BU,0xA2U,0xCEU,0x96U,0xA3U,0x61U,0x05U,0x3FU,0xFFU,0x81U,
                0xE9U
            };

            // ed25519
            uint8_t pubkey_ed[] =
            {
                0xEDU,0xD9U,0xB3U,0x59U,0x98U,0x02U,0xB2U,0x14U,0xA9U,0x9DU,
                0x75U,0x77U,0x12U,0xD6U,0xABU,0xDFU,0x72U,0xF8U,0x3CU,0x63U,
                0xBBU,0xD5U,0x38U,0x61U,0x41U,0x17U,0x90U,0xB1U,0x3DU,0x04U,
                0xB2U,0xC5U,0xC9U
            };

            uint8_t sig_ed[] =
            {
                0x56U,0x68U,0x80U,0x76U,0x70U,0xFEU,0xCEU,0x60U,0x34U,0xAFU,
                0xD6U,0xCDU,0x1BU,0xB4U,0xC6U,0x60U,0xAEU,0x08U,0x39U,0x6DU,
                0x6DU,0x8BU,0x7DU,0x22U,0x71U,0x3BU,0xDAU,0x26U,0x43U,0xC1U,
                0xE1U,0x91U,0xC4U,0xE4U,0x4DU,0x8EU,0x02U,0xE8U,0x57U,0x8BU,
                0x20U,0x45U,0xDAU,0xD4U,0x8FU,0x97U,0xFCU,0x16U,0xF8U,0x92U,
                0x5BU,0x6BU,0x51U,0xFBU,0x3BU,0xE5U,0x0FU,0xB0U,0x4BU,0x3AU,
                0x20U,0x4CU,0x53U,0x04U
            };


            uint8_t msg[] =
            {
                0xDEU,0xADU,0xBEU,0xEFU
            };


            int64_t hook(uint32_t reserved )
            {
                _g(1,1);

                // Test out of bounds check
                ASSERT(util_verify(1000000, 33, 0, 20, 0, 20) == OUT_OF_BOUNDS);
                ASSERT(util_verify(0, 33, 10000000, 20, 0, 20) == OUT_OF_BOUNDS);
                ASSERT(util_verify(0, 33, 0, 20, 10000000, 20) == OUT_OF_BOUNDS);
                
                ASSERT(util_verify(0, 1000000, 33, 1, 20, 30) == OUT_OF_BOUNDS);
                ASSERT(util_verify(0, 33, 0, 10000000, 20, 30) == OUT_OF_BOUNDS);
                ASSERT(util_verify(0, 33, 0, 2, 20, 10000000) == OUT_OF_BOUNDS);

                ASSERT(util_verify(0, 30, 0, 1, 0, 30) == INVALID_KEY);
                ASSERT(util_verify(0, 33, 0, 0, SBUF(pubkey_sec)) == TOO_SMALL);
                ASSERT(util_verify(0, 0, 0, 100, SBUF(pubkey_sec)) == TOO_SMALL);

                // test secp256k1 verification
                ASSERT(util_verify(SBUF(msg), SBUF(sig_sec), SBUF(pubkey_sec)) == 1);
                ASSERT(util_verify(msg + 1, sizeof(msg) - 1, SBUF(sig_sec), SBUF(pubkey_sec)) == 0);

                // test ed25519 verification
                ASSERT(util_verify(SBUF(msg), SBUF(sig_ed), SBUF(pubkey_ed)) == 1);
                ASSERT(util_verify(msg + 1, sizeof(msg) - 1, SBUF(sig_ed), SBUF(pubkey_ed)) == 0);

                accept(0,0,0);
            }
        )[test.hook]"];

        // install the hook on alice
        env(ripple::test::jtx::hook(alice, {{hso(hook, overrideFlag)}}, 0),
            M("set util_verify"),
            HSFEE);
        env.close();

        // invoke the hook
        env(pay(bob, alice, XRP(1)), M("test util_verify"), fee(XRP(1)));
    }

    void
    run() override
    {
        // testTicketSetHook();  // RH TODO
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

        test_emit();                //
        //test_etxn_burden();       // tested above
        //test_etxn_generation();   // tested above
        //test_otxn_burden();       // tested above
        //test_otxn_generation();   // tested above
        test_etxn_details();        //
        test_etxn_fee_base();       //
        test_etxn_nonce();          //
        test_etxn_reserve();        //
        test_fee_base();            //

        test_otxn_field();          //

        test_ledger_keylet();       //

        test_float_compare();       //
        test_float_divide();        //
        test_float_int();           //
        test_float_invert();        //
        test_float_log();           //
        test_float_mantissa();      //
        test_float_mulratio();      //
        test_float_multiply();      //
        test_float_negate();        //
        test_float_one();           //
        test_float_root();          //
        test_float_set();           //
        test_float_sign();          //
        test_float_sto();           //
        test_float_sto_set();       //
        test_float_sum();           //

        test_hook_account();        //
        test_hook_again();          //
        test_hook_hash();           //
        test_hook_param();          //
        test_hook_param_set();      //
        test_hook_pos();            //
        test_hook_skip();           //

        test_ledger_last_hash();    //
        test_ledger_last_time();    //
        test_ledger_nonce();        //
        test_ledger_seq();          //

        test_meta_slot();           //

        test_otxn_id();             //
        test_otxn_slot();           //
        test_otxn_type();           //
        test_otxn_param();          // 

        test_slot();                //
        test_slot_clear();          //
        test_slot_count();          //
        test_slot_float();          //
        test_slot_set();            //
        test_slot_size();           //
        test_slot_subarray();       //
        test_slot_subfield();       //
        test_slot_type();           //

        test_state();               //
        test_state_foreign();       //
        test_state_foreign_set();   // 
        test_state_set();           //

        test_sto_emplace();         //
        test_sto_erase();           //
        test_sto_subarray();        //
        test_sto_subfield();        //
        test_sto_validate();        //

        test_trace();               //
        test_trace_float();         //
        test_trace_num();           //

        test_util_accid();          //
        test_util_keylet();         //
        test_util_raddr();          //
        test_util_sha512h();        //
        test_util_verify();         //
    }

private:

    TestHook accept_wasm =  // WASM: 0
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
        )[test.hook]"];

    HASH_WASM(accept);

    TestHook rollback_wasm =  // WASM: 1
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
        )[test.hook]"];

    HASH_WASM(rollback);

    TestHook noguard_wasm =  // WASM: 2
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
        )[test.hook]"];

    TestHook illegalfunc_wasm =  // WASM: 3
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
        )[test.hook]"];

    TestHook long_wasm =  // WASM: 4
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
        )[test.hook]"];

    TestHook makestate_wasm =  // WASM: 5
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
        )[test.hook]"];

    HASH_WASM(makestate);

    // this is just used as a second small hook with a unique hash
    TestHook accept2_wasm =  // WASM: 6
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
        )[test.hook]"];

    HASH_WASM(accept2);
};
BEAST_DEFINE_TESTSUITE(SetHook, tx, ripple);
}  // namespace test
}  // namespace ripple
