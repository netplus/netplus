#ifndef _NETP_HANDLER_TLS_CREDENTIALS_HPP
#define  _NETP_HANDLER_TLS_CREDENTIALS_HPP

#include <memory>
#include <netp/app.hpp>

#ifdef NETP_WITH_BOTAN
#include <botan/credentials_manager.h>
#include <botan/certstor_system.h>
#include <botan/pkcs8.h>
#include <botan/data_src.h>

namespace netp {  namespace handler {

	inline bool value_exists(const std::vector<std::string>& vec,
		const std::string& val)
	{
		for (size_t i = 0; i != vec.size(); ++i)
		{
			if (vec[i] == val)
			{
				return true;
			}
		}
		return false;
	}

    class Basic_Credentials_Manager : public Botan::Credentials_Manager
    {
        void __load_system_cert_store(bool use_system_store ) {
#if defined(BOTAN_HAS_CERTSTOR_SYSTEM)
            if (use_system_store) {
                try {
                    m_certstores.push_back(std::make_shared<Botan::System_Certificate_Store>());
                } catch (Botan::Exception& e) {
                    NETP_ERR("[tls_credential]load system store ca failed, %d: %s", e.error_code(), e.what() );
                } catch (std::exception& e) {
                    NETP_ERR("[tls_credential]load system store ca failed, %s", e.what());
                } catch (...) {
                    NETP_ERR("[tls_credential]load system store ca failed, unknown error");
                }
            }
#else
            BOTAN_UNUSED(use_system_store);
#endif
        }

        void __load_cert_by_path(const std::string& ca_path) {
            if (ca_path.empty() == false)
            {
                m_certstores.push_back(std::make_shared<Botan::Certificate_Store_In_Memory>(ca_path));
            }
        }

    public:
        Basic_Credentials_Manager(bool use_system_store, const std::string& ca_path)
        {
            __load_system_cert_store(use_system_store);
            __load_cert_by_path(ca_path);
        }

        Basic_Credentials_Manager(bool use_system_store, const std::string& ca_path, Botan::RandomNumberGenerator& rng, const std::string& cert, const std::string& priv_key) {
            __load_system_cert_store(use_system_store);
            __load_cert_by_path(ca_path);
            __load_cert(rng, cert, priv_key);
        }

        void __load_cert(Botan::RandomNumberGenerator& rng,
            const std::string& server_crt,
            const std::string& server_key) 
        {
            if (server_crt.empty() || server_key.empty()) {
                return;
            }

            Certificate_Info cert;
            try {
                cert.key.reset(Botan::PKCS8::load_key(server_key, rng));
            }
            catch (std::exception&) {
            }

            Botan::DataSource_Stream in(server_crt);
            while (!in.end_of_data())
            {
                try
                {
                    cert.certs.push_back(Botan::X509_Certificate(in));
                    //NETP_VERBOSE("[tls_credentials]add cert: %s", server_crt.c_str() );
                }
                catch (std::exception&) {
                }
            }

            // TODO: attempt to validate chain ourselves

            m_creds.push_back(cert);
        }

        Basic_Credentials_Manager(Botan::RandomNumberGenerator& rng,
            const std::string& server_crt,
            const std::string& server_key)
        {
            __load_cert(rng, server_crt, server_key);
        }

        std::vector<Botan::Certificate_Store*>
            trusted_certificate_authorities(const std::string& type,
                const std::string& /*hostname*/) override
        {
            (void)type;
            std::vector<Botan::Certificate_Store*> v;
            //NETP_VERBOSE("[tls_credentials]load trusted_certificate_authorities for : %s", type.c_str() );
            // don't ask for client certs
            //if (type == "tls-server")
            //{
            //    return v;
            //}

            for (auto const& cs : m_certstores)
            {
                v.push_back(cs.get());
            }

            return v;
        }

        std::vector<Botan::X509_Certificate> cert_chain(
            const std::vector<std::string>& algos,
            const std::string& type,
            const std::string& hostname) override
        {
            BOTAN_UNUSED(type);

            for (auto const& i : m_creds)
            {
                if (std::find(algos.begin(), algos.end(), i.key->algo_name()) == algos.end())
                {
                    continue;
                }

                if (hostname != "" && !i.certs[0].matches_dns_name(hostname))
                {
                    continue;
                }

                return i.certs;
            }

            return std::vector<Botan::X509_Certificate>();
        }

        Botan::Private_Key* private_key_for(const Botan::X509_Certificate& cert,
            const std::string& /*type*/,
            const std::string& /*context*/) override
        {
            for (auto const& i : m_creds)
            {
                if (cert == i.certs[0])
                {
                    return i.key.get();
                }
            }

            return nullptr;
        }

    private:
        struct Certificate_Info
        {
            std::vector<Botan::X509_Certificate> certs;
            std::shared_ptr<Botan::Private_Key> key;
        };

        std::vector<Certificate_Info> m_creds;
        std::vector<std::shared_ptr<Botan::Certificate_Store>> m_certstores;
    };
}}

#endif

#endif