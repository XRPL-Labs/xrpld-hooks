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


        // check blank grants
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            iv[jss::HookGrants] = Json::Value{Json::arrayValue};
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("HSO must include at least one entry in HookGrants when field is present"),
                HSFEE, ter(temMALFORMED));
            env.close();
        }

        // check too many grants
        {
            Json::Value iv;
            iv[jss::HookHash] = to_string(uint256{beast::zero});
            Json::Value grants {Json::arrayValue};
            for (uint32_t i = 0; i < 9; ++i)
            {
                Json::Value pv;
                Json::Value piv;
                piv[jss::HookHash] = to_string(uint256{beast::zero});
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

        std::string accept_hash = to_string(ripple::sha512Half_s(
            ripple::Slice(accept_wasm.data(), accept_wasm.size())
        ));
        
        std::string rollback_hash = to_string(ripple::sha512Half_s(
            ripple::Slice(accept_wasm.data(), accept_wasm.size())
        ));

        Json::Value jv;
        jv[jss::Account] = alice.human();
        jv[jss::TransactionType] = jss::SetHook;
        jv[jss::Flags] = 0;
        jv[jss::Hooks] = Json::Value{Json::arrayValue};

        // can't set api version
        {
            Json::Value iv;
            iv[jss::HookHash] = accept_hash;
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
            iv[jss::HookHash] = accept_hash;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation can set extant hook hash"),
                HSFEE, ter(tesSUCCESS));
            env.close();
        }

        // can't set extant hook over other hook without override flag
        {
            Json::Value iv;
            iv[jss::HookHash] = rollback_hash;
            jv[jss::Hooks][0U][jss::Hook] = iv;
            env(jv,
                M("Hook Install operation can set extant hook hash"),
                HSFEE, ter(tecREQUIRES_FLAG));
            env.close();
        }

        // can set extant hook over other hook with override flag
        {
            Json::Value iv;
            iv[jss::HookHash] = rollback_hash;
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

        // create a namespace
        std::string ns = "CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE";
        {
            // create hook
            Json::Value jv = 
                ripple::test::jtx::hook(alice, {{hso(makestate_wasm)}}, 0);

            jv[jss::Hooks][0U][jss::Hook][jss::HookNamespace] = ns;
            env(jv, M("Create makestate hook"), HSFEE, ter(tesSUCCESS));
            env.close();

            // run hook
            env(pay(bob, alice, XRP(1)),
                M("Run create state hook"),
                fee(XRP(1)));
            env.close();

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

            auto const hookstate = env.le(
                keylet::hookState(
                    Account("alice").id(),
                    key,
                    ns)
            );

            // check if the hookstate object was created
            BEAST_EXPECT(!!hookstate);

            // check if the value was set correctly
            auto const& data = hookstate->getFieldVL(sfHookStateData);
            BEAST_EXPECT(data.size() == 6);
            BEAST_EXPECT(
                    data[0] == 'v' && data[1] == 'a' && data[2] == 'l' &&
                    data[3] == 'u' && data[4] == 'e' && data[5] == '\0');
        }

        // RH UPTO
        // delete the namespace

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
        
            
        auto const accept_hash = ripple::sha512Half_s(
            ripple::Slice(accept_wasm.data(), accept_wasm.size())
        );

        // correctly formed
        {
            Json::Value jv =
                ripple::test::jtx::hook(alice, {{hso(accept_wasm)}}, 0);
            env(jv,
                M("Normal accept"),
                HSFEE, ter(tesSUCCESS));
            env.close();

            auto const def = env.le(keylet::hookDefinition(accept_hash));
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

            auto const def = env.le(keylet::hookDefinition(accept_hash));
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

            auto const rollback_def = env.le(keylet::hookDefinition(rollback_hash));
            auto const accept_def = env.le(keylet::hookDefinition(accept_hash));
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
    testAccept()
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
    testRollback()
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
        testAccept();
        testRollback();
    }

private:

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
