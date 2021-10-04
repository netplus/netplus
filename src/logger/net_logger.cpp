#include <netp/event_loop.hpp>
#include <netp/rpc.hpp>
#include <netp/logger/net_logger.hpp>
#include <netp/app.hpp>

namespace netp { namespace logger {

	void net_logger::_tm_redial(NRP<timer> const& tm) {
		NETP_ASSERT(m_loop->in_event_loop());
		//dial until close called or connected, 
		if (m_flag&(u8_t(flag::f_close_called)|u8_t(flag::f_connected))) {
			return;
		}

		if ((m_flag&u8_t(flag::f_connecting)) == 0) {
			_do_dial(netp::make_ref<promise<int>>());
		}
		m_loop->launch(tm, netp::make_ref<promise<int>>());
	}

	void net_logger::_do_dial(NRP<promise<int>> const& p) {
		NETP_ASSERT(m_loop->in_event_loop());
		if (m_flag & (u8_t(flag::f_connected)|u8_t(flag::f_connecting)|u8_t(flag::f_close_called)) ) {
			p->set(netp::E_INVALID_STATE);
			return;
		}

		NETP_ASSERT(m_rpc == nullptr);
		m_flag = u8_t(flag::f_connecting);
		NRP<socket_cfg> cfg = netp::make_ref<socket_cfg>(m_loop);
		NRP<rpc_dial_promise> rpc_dp = rpc::dial(m_server.c_str(), m_server.length(),nullptr, cfg);

		rpc_dp->if_done([logger=NRP<net_logger>(this),p](std::tuple<int, NRP<netp::rpc>> const& tupr) {
			logger->m_flag &= ~u8_t(flag::f_connecting);

			int rt = std::get<0>(tupr);
			if (rt != netp::OK) {
				p->set(rt);
				NRP<timer> tm_redial = netp::make_ref<timer>(std::chrono::seconds(1), &net_logger::_tm_redial, logger, std::placeholders::_1);
				logger->m_loop->launch(tm_redial, netp::make_ref<promise<int>>());
				return;
			}

			if (logger->m_flag&u8_t(flag::f_close_called)) {
				std::get<1>(tupr)->close();
				p->set(netp::E_RPC_CONNECT_ABORT);
				return;
			}
			logger->m_flag |= u8_t(flag::f_connected);
			logger->m_rpc = std::get<1>(tupr);

			logger->m_rpc->close_promise()->if_done([logger]( int const& ) {
				logger->m_flag &= ~u8_t(flag::f_connected);
				logger->m_rpc = nullptr;
				NRP<timer> tm_redial = netp::make_ref<timer>(std::chrono::seconds(1), &net_logger::_tm_redial, logger, std::placeholders::_1);
				logger->m_loop->launch(tm_redial, netp::make_ref<promise<int>>());
			});

			p->set(netp::OK);
			if (logger->m_loglist.size()) {
				logger->_do_push();
			}
		});
	}
	void net_logger::_do_close(NRP<promise<int>> const& p) {
		if ((m_flag&u8_t(flag::f_connected)) == 0) {
			p->set(netp::E_INVALID_STATE);
			return;
		}

		NETP_ASSERT(m_rpc != nullptr);
		m_flag &= ~u8_t(flag::f_connected);
		m_rpc->close();
		m_rpc = nullptr;
	}

	void net_logger::_do_push_done(int rt) {
		NETP_ASSERT(rt != netp::E_CHANNEL_WRITE_BLOCK);
		NETP_ASSERT(m_flag& u8_t(flag::f_writing));
		m_flag &= ~u8_t(flag::f_writing);
		if (rt == netp::OK) {
			NETP_ASSERT(m_loglist.size());
			m_loglist.pop_front();
			if (m_loglist.size()) {
				_do_push();
			} else {
				std::list<NRP<packet>>().swap(m_loglist);
			}
			return;
		}
		_do_close(netp::make_ref<promise<int>>());
	}

	void net_logger::_do_push() {
		NETP_ASSERT(m_loop->in_event_loop());

		if ( (m_flag&u8_t(flag::f_connected)) ==0 ) {
			return;
		}

		if (m_flag&u8_t(flag::f_writing)) {
			return;
		}

		NETP_ASSERT(m_rpc != nullptr);
		m_flag |= u8_t(flag::f_writing);
		NRP<promise<int>> pushp = netp::make_ref<promise<int>>();
		pushp->if_done([logger = NRP<net_logger>(this)](int const& rt) {
			logger->_do_push_done(rt);
		});
		NETP_ASSERT(m_loglist.size());
		m_rpc->do_push(pushp,m_loglist.front());
	}

	net_logger::net_logger(std::string const& server) :
		m_loop(app::instance()->def_loop_group()->next()),
		m_flag(0),
		m_server(server.c_str(), server.length()),
		m_rpc()
	{
	}

	net_logger::~net_logger() {
	}

	void net_logger::write( log_mask mask, char const* log, netp::u32_t len ) {
		NETP_ASSERT(test_mask(mask));
		NRP<netp::packet> logp = netp::make_ref<netp::packet>(log, len);
		m_loop->execute([logger=NRP<net_logger>(this), logp]() {
			logger->m_loglist.push_back(logp);
			logger->_do_push();
		});
	}

	NRP<promise<int>> net_logger::dial() {
		NRP<promise<int>> p = netp::make_ref<promise<int>>();
		m_loop->execute([logger=NRP<net_logger>(this),p]() {
			logger->_do_dial(p);
		});
		return p;
	}

	NRP<promise<int>> net_logger::close() {
		NRP<promise<int>> p = netp::make_ref<promise<int>>();
		m_loop->execute([logger=NRP<net_logger>(this),p]() {
			if (logger->m_flag&u8_t(flag::f_close_called)) {
				p->set(netp::E_INVALID_STATE);
				return;
			}
			logger->m_flag |= u8_t(flag::f_close_called);
			logger->_do_close(p);
		});
		return p;
	}
}}