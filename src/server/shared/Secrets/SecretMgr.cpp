/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SecretMgr.h"
#include "AES.h"
#include "Argon2.h"
#include "Config.h"
#include "CryptoGenerics.h"
#include "DatabaseEnv.h"
#include "Errors.h"
#include "Log.h"
#include "SharedDefines.h"

#define SECRET_FLAG_FOR(key, val, server) server ## _ ## key = (val ## ull << (16*SERVER_PROCESS_ ## server))
#define SECRET_FLAG(key, val) SECRET_FLAG_ ## key = val, SECRET_FLAG_FOR(key, val, AUTHSERVER), SECRET_FLAG_FOR(key, val, WORLDSERVER)
enum SecretFlags : uint64
{
    SECRET_FLAG(DEFER_LOAD, 0x1)
};
#undef SECRET_FLAG_FOR
#undef SECRET_FLAG

struct SecretInfo
{
    char const* configKey;
    char const* oldKey;
    int bits;
    ServerProcessTypes owner;
    uint64 _flags;
    [[nodiscard]] uint16 flags() const { return static_cast<uint16>(_flags >> (16*THIS_SERVER_PROCESS)); }
};

static constexpr SecretInfo secret_info[NUM_SECRETS] =
{
    { "TOTPMasterSecret", "TOTPOldMasterSecret", 128, SERVER_PROCESS_AUTHSERVER, WORLDSERVER_DEFER_LOAD }
};

/*static*/ SecretMgr* SecretMgr::instance()
{
    static SecretMgr instance;
    return &instance;
}

static Optional<BigNumber> GetHexFromConfig(char const* configKey, int bits)
{
    ASSERT(bits > 0);
    auto str = sConfigMgr->GetOption<std::string>(configKey, "");
    if (str.empty())
        return {};

    BigNumber secret;
    if (!secret.SetHexStr(str.c_str()))
    {
        LOG_FATAL("server.loading", "Invalid value for '{}' - specify a hexadecimal integer of up to {} bits with no prefix.", configKey, bits);
        ABORT();
    }

    BigNumber threshold(2);
    threshold <<= bits;
    if (!((BigNumber(0) <= secret) && (secret < threshold)))
    {
        LOG_ERROR("server.loading", "Value for '{}' is out of bounds (should be an integer of up to {} bits with no prefix). Truncated to {} bits.", configKey, bits, bits);
        secret %= threshold;
    }
    ASSERT(((BigNumber(0) <= secret) && (secret < threshold)));

    return secret;
}

void SecretMgr::Initialize()
{
    for (uint32 i = 0; i < NUM_SECRETS; ++i)
    {
        if (secret_info[i].flags() & SECRET_FLAG_DEFER_LOAD)
            continue;
        std::unique_lock<std::mutex> lock(_secrets[i].lock);
        AttemptLoad(Secrets(i), Warhead::LogLevel::Fatal, lock);
        if (!_secrets[i].IsAvailable())
            ABORT(); // load failed
    }
}

SecretMgr::Secret const& SecretMgr::GetSecret(Secrets i)
{
    std::unique_lock<std::mutex> lock(_secrets[i].lock);

    if (_secrets[i].state == Secret::NOT_LOADED_YET)
        AttemptLoad(i, Warhead::LogLevel::Fatal, lock);
    return _secrets[i];
}

void SecretMgr::AttemptLoad(Secrets i, Warhead::LogLevel errorLevel, std::unique_lock<std::mutex> const&)
{
    auto const& info = secret_info[i];

    Optional<std::string> oldDigest;
    {
        auto stmt = AuthDatabase.GetPreparedStatement(LOGIN_SEL_SECRET_DIGEST);
        stmt->SetArguments(i);

        if (auto result = AuthDatabase.Query(stmt))
            oldDigest = result->Fetch()->Get<std::string>();
    }

    Optional<BigNumber> currentValue = GetHexFromConfig(info.configKey, info.bits);

    // verify digest
    if (
        ((!oldDigest) != (!currentValue)) || // there is an old digest, but no current secret (or vice versa)
        (oldDigest && !Warhead::Crypto::Argon2::Verify(currentValue->AsHexStr(), *oldDigest)) // there is an old digest, and the current secret does not match it
        )
    {
        if (info.owner != THIS_SERVER_PROCESS)
        {
            if (currentValue)
                LOG_MSG_BODY("server.loading", errorLevel, "Invalid value for '{}' specified - this is not actually the secret being used in your auth DB.", info.configKey);
            else
                LOG_MSG_BODY("server.loading", errorLevel, "No value for '{}' specified - please specify the secret currently being used in your auth DB.", info.configKey);
            _secrets[i].state = Secret::LOAD_FAILED;
            return;
        }

        Optional<BigNumber> oldSecret;
        if (oldDigest && info.oldKey) // there is an old digest, so there might be an old secret (if possible)
        {
            oldSecret = GetHexFromConfig(info.oldKey, info.bits);
            if (oldSecret && !Warhead::Crypto::Argon2::Verify(oldSecret->AsHexStr(), *oldDigest))
            {
                LOG_MSG_BODY("server.loading", errorLevel, "Invalid value for '{}' specified - this is not actually the secret previously used in your auth DB.", info.oldKey);
                _secrets[i].state = Secret::LOAD_FAILED;
                return;
            }
        }

        // attempt to transition us to the new key, if possible
        Optional<std::string> error = AttemptTransition(Secrets(i), currentValue, oldSecret, static_cast<bool>(oldDigest));
        if (error)
        {
            LOG_MSG_BODY("server.loading", errorLevel, "Your value of '{}' changed, but we cannot transition your database to the new value:\n{}", info.configKey, error->c_str());
            _secrets[i].state = Secret::LOAD_FAILED;
            return;
        }

        LOG_INFO("server.loading", "Successfully transitioned database to new '{}' value.", info.configKey);
    }

    if (currentValue)
    {
        _secrets[i].state = Secret::PRESENT;
        _secrets[i].value = *currentValue;
    }
    else
        _secrets[i].state = Secret::NOT_PRESENT;
}

Optional<std::string> SecretMgr::AttemptTransition(Secrets i, Optional<BigNumber> const& newSecret, Optional<BigNumber> const& oldSecret, bool hadOldSecret) const
{
    AuthDatabaseTransaction trans = AuthDatabase.BeginTransaction();

    switch (i)
    {
        case SECRET_TOTP_MASTER_KEY:
        {
            auto result = AuthDatabase.Query("SELECT id, totp_secret FROM account");
            if (result)
            {
                for (auto& row : *result)
                {
                    if (row[0].IsNull())
                        continue;

                    auto id = row[0].Get<uint32>();
                    auto totpSecret = row[1].Get<Binary>();

                    if (hadOldSecret)
                    {
                        if (!oldSecret)
                            return Warhead::StringFormat("Cannot decrypt old TOTP tokens - add config key '{}' to authserver.conf!", secret_info[i].oldKey);

                        if (!Warhead::Crypto::AEDecrypt<Warhead::Crypto::AES>(totpSecret, oldSecret->ToByteArray<Warhead::Crypto::AES::KEY_SIZE_BYTES>()))
                            return Warhead::StringFormat("Cannot decrypt old TOTP tokens - value of '{}' is incorrect for some users!", secret_info[i].oldKey);
                    }

                    if (newSecret)
                        Warhead::Crypto::AEEncryptWithRandomIV<Warhead::Crypto::AES>(totpSecret, newSecret->ToByteArray<Warhead::Crypto::AES::KEY_SIZE_BYTES>());

                    auto updateStmt = AuthDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_TOTP_SECRET);
                    updateStmt->SetArguments(totpSecret, id);
                    trans->Append(updateStmt);
                }
            }
            break;
        }
        default:
            return std::string{"Unknown secret index - huh?"};
    }

    if (hadOldSecret)
    {
        auto deleteStmt = AuthDatabase.GetPreparedStatement(LOGIN_DEL_SECRET_DIGEST);
        deleteStmt->SetArguments(i);
        trans->Append(deleteStmt);
    }

    if (newSecret)
    {
        BigNumber salt;
        salt.SetRand(128);
        auto hash = Warhead::Crypto::Argon2::Hash(newSecret->AsHexStr(), salt);
        if (!hash)
            return std::string("Failed to hash new secret");

        auto insertStmt = AuthDatabase.GetPreparedStatement(LOGIN_INS_SECRET_DIGEST);
        insertStmt->SetArguments(i, *hash);
        trans->Append(insertStmt);
    }

    AuthDatabase.CommitTransaction(trans);
    return {};
}
