#ifndef _NETP_TRAFFIC_REPEATER_HPP
#define _NETP_TRAFFIC_REPEATER_HPP


#include <functional>
#include <deque>
#include <netp/core.hpp>
#include <netp/packet.hpp>
#include <netp/event_broker.hpp>
#include <netp/io_event_loop.hpp>

#define NETP_TRAFFIC_REPEATER_BUF_MAX (1024*1024*8)
#define NETP_TRAFFIC_REPEATER_BUF_MIN (1024*512)
#define NETP_TRAFFIC_REPEATER_BUF_DEF (1024*1024*2)

namespace netp { namespace traffic {

	//design goal:
	//1, any interface capable of write can use this utility

	typedef std::function<void()> fn_repeater_event_t;
	typedef std::function<void(int err)> fn_repeater_error_event_t;

	enum repeater_event {
		e_write_error,
		e_buffer_full,
		e_buffer_empty,
		e_finished
	};

	template <typename _dst_writer>
	class repeater final:
		public netp::ref_base,
		public netp::event_broker_any
	{
		typedef _dst_writer _repeater_writer_t;
		typedef repeater<_dst_writer>	_self_t;
		typedef std::deque<NRP<netp::packet>,netp::allocator<NRP<netp::packet>>> repeater_outlet_q_t;
		enum class repeater_state {
			S_IDLE,
			S_WRITING,
			S_FINISHED
		};

		NRP<netp::io_event_loop> m_loop;
		_repeater_writer_t m_writer;
		netp::size_t m_bufsize;

		repeater_outlet_q_t m_outlets;
		netp::size_t m_outlets_nbytes;
		repeater_state m_state;

		bool m_buffer_full;
		bool m_mark_finished;

		void _flush_done(int rt) {
			if (rt != netp::OK) {
				NETP_ASSERT(rt != netp::E_CHANNEL_WRITE_BLOCK);
				event_broker_any::invoke<fn_repeater_error_event_t>(repeater_event::e_write_error, rt);
				m_state = repeater_state::S_FINISHED;
				return;
			}

			NETP_ASSERT(m_outlets.size() && m_state == repeater_state::S_WRITING);
			m_outlets_nbytes -= m_outlets.front()->len();
			m_outlets.pop_front();
			m_state = repeater_state::S_IDLE;

			if (m_outlets.size()) {
				_do_flush();
				return;
			}
			NETP_ASSERT(m_outlets_nbytes == 0);
			repeater_outlet_q_t().swap(m_outlets);
			if (m_buffer_full) {
				m_buffer_full = false;
				event_broker_any::invoke<fn_repeater_event_t>(repeater_event::e_buffer_empty);
			}
			if (m_mark_finished) {
				m_state = repeater_state::S_FINISHED;
				event_broker_any::invoke<fn_repeater_event_t>(repeater_event::e_finished);
			}
		}
		void _do_flush() {
			NETP_ASSERT(m_loop->in_event_loop());
			if (m_state != repeater_state::S_IDLE) {
				return;
			}

			if (m_outlets.size()) {
				NRP<netp::promise<int>> wf = netp::make_ref<netp::promise<int>>();
				wf->if_done([L = m_loop, r = NRP<_self_t>(this)](int const& rt) {
					L->execute([r, rt]() {
						r->_flush_done(rt);
					});
				});
				m_state = repeater_state::S_WRITING;
				m_writer->write(m_outlets.front(), wf);
			}
		}

	public:
		repeater(NRP<netp::io_event_loop> const& L_, _repeater_writer_t const& writer_, netp::size_t bufsize_ = NETP_TRAFFIC_REPEATER_BUF_DEF ) :
			m_loop(L_),
			m_writer(writer_),
			m_bufsize(bufsize_>NETP_TRAFFIC_REPEATER_BUF_MAX?NETP_TRAFFIC_REPEATER_BUF_MAX: bufsize_ < NETP_TRAFFIC_REPEATER_BUF_MIN? NETP_TRAFFIC_REPEATER_BUF_MIN:bufsize_),
			m_outlets_nbytes(0),
			m_state(repeater_state::S_IDLE),
			m_buffer_full(false),
			m_mark_finished(false)
		{
		}

		void relay(NRP<netp::packet> const& outp) {
			if (!m_loop->in_event_loop()) {
				m_loop->schedule([rep = NRP<_self_t>(this), outp]() {
					rep->relay(outp);
				});
				return;
			}

			if (m_state == repeater_state::S_FINISHED) {
				event_broker_any::invoke<fn_repeater_error_event_t>(repeater_event::e_write_error, netp::E_INVALID_STATE);
				return;
			}

			NETP_ASSERT(outp != nullptr && outp->len());
			m_outlets.push_back(std::move(outp));
			m_outlets_nbytes += outp->len();

			if (m_outlets_nbytes > m_bufsize) {
				m_buffer_full = true;
				event_broker_any::invoke<fn_repeater_event_t>(repeater_event::e_buffer_full);
			}
			_do_flush();
		}

		void finish() {
			if (!m_loop->in_event_loop()) {
				m_loop->schedule([rep = NRP<_self_t>(this)]() {
					rep->finish();
				});
				return;
			}

			m_mark_finished = true;
			if (m_state == repeater_state::S_FINISHED) {
				//error or marked already
				return;
			}

			//all write error would be triggered during write operation, so we only need to mark state here
			if ((m_outlets.size() == 0)) {
				NETP_ASSERT(m_state == repeater_state::S_IDLE);
				m_state = repeater_state::S_FINISHED;
				event_broker_any::invoke<fn_repeater_event_t>(repeater_event::e_finished);
			}
		}
	};
}}

#endif