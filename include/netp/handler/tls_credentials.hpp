#ifndef _NETP_HANDLER_TLS_CREDENTIALS_HPP
#define  _NETP_HANDLER_TLS_CREDENTIALS_HPP

#include <memory>

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
    public:
        Basic_Credentials_Manager(bool use_system_store,
            const std::string& ca_path)
        {
            if (ca_path.empty() == false)
            {
                m_certstores.push_back(std::make_shared<Botan::Certificate_Store_In_Memory>(ca_path));
            }

#if defined(BOTAN_HAS_CERTSTOR_SYSTEM)
            if (use_system_store)
            {
                m_certstores.push_back(std::make_shared<Botan::System_Certificate_Store>());
            }
#else
            BOTAN_UNUSED(use_system_store);
#endif
        }

        Basic_Credentials_Manager(Botan::RandomNumberGenerator& rng,
            const std::string& server_crt,
            const std::string& server_key)
        {
            Certificate_Info cert;

            cert.key.reset(Botan::PKCS8::load_key(server_key, rng));

            Botan::DataSource_Stream in(server_crt);
            while (!in.end_of_data())
            {
                try
                {
                    cert.certs.push_back(Botan::X509_Certificate(in));
                }
                catch (std::exception&)
                {
                }
            }

            // TODO: attempt to validate chain ourselves

            m_creds.push_back(cert);
        }

        std::vector<Botan::Certificate_Store*>
            trusted_certificate_authorities(const std::string& type,
                const std::string& /*hostname*/) override
        {
            std::vector<Botan::Certificate_Store*> v;

            // don't ask for client certs
            if (type == "tls-server")
            {
                return v;
            }

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