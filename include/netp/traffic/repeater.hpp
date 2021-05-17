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
		enum repeater_flag {
			f_writing =1<<1,
			f_finished = 1<<2,
			f_finish_pending=1<<3,
			f_buf_full = 1<<4
		};

		NRP<netp::io_event_loop> m_loop;
		_repeater_writer_t m_writer;
		int m_flag;
		netp::size_t m_bufsize;
		netp::size_t m_outlets_nbytes;
		repeater_outlet_q_t m_outlets;


		void _do_write_done(int rt) {
			NETP_ASSERT(m_outlets.size() && (m_flag&f_writing) );
			m_flag &= ~f_writing;

			if (rt != netp::OK) {
				NETP_ASSERT(rt != netp::E_CHANNEL_WRITE_BLOCK);
				m_flag &= ~f_finish_pending; //if any
				m_flag |= f_finished;
				event_broker_any::invoke<fn_repeater_error_event_t>(repeater_event::e_write_error, rt);
				return;
			}

			m_outlets_nbytes -= m_outlets.front()->len();
			m_outlets.pop_front();

			if (m_outlets.size()) {
				_do_write();
				return;
			}

			NETP_ASSERT(m_outlets_nbytes == 0);
			repeater_outlet_q_t().swap(m_outlets);
			if (m_flag&f_buf_full) {
				m_flag &= ~f_buf_full;
				event_broker_any::invoke<fn_repeater_event_t>(repeater_event::e_buffer_empty);
			}

			if (m_flag&f_finish_pending) {
				m_flag &= ~f_finish_pending;
				m_flag |= f_finished;
				event_broker_any::invoke<fn_repeater_event_t>(repeater_event::e_finished);
			}
		}
		void _do_write() {
			NETP_ASSERT(m_loop->in_event_loop());
			if ( m_flag&f_writing ) {
				return;
			}

			if (m_outlets.size()) {
				NRP<netp::promise<int>> wp = netp::make_ref<netp::promise<int>>();
				wp->if_done([L = m_loop, r = NRP<_self_t>(this)](int const& rt) {
					L->execute([r, rt]() {
						r->_do_write_done(rt);
					});
				});
				m_flag |= f_writing;
				m_writer->write(wp,m_outlets.front());
			}
		}

	public:
		repeater(NRP<netp::io_event_loop> const& L_, _repeater_writer_t const& writer_, netp::size_t bufsize_ = NETP_TRAFFIC_REPEATER_BUF_DEF ) :
			m_loop(L_),
			m_writer(writer_),
			m_flag(0),
			m_outlets_nbytes(0),
			m_bufsize(bufsize_>NETP_TRAFFIC_REPEATER_BUF_MAX?NETP_TRAFFIC_REPEATER_BUF_MAX: bufsize_ < NETP_TRAFFIC_REPEATER_BUF_MIN? NETP_TRAFFIC_REPEATER_BUF_MIN:bufsize_)
		{
		}

		void relay(NRP<netp::packet> const& outp) {
			if (!m_loop->in_event_loop()) {
				m_loop->schedule([rep = NRP<_self_t>(this), outp]() {
					rep->relay(outp);
				});
				return;
			}

			if (m_flag&(f_finished|f_finish_pending)) {
				event_broker_any::invoke<fn_repeater_error_event_t>(repeater_event::e_write_error, netp::E_INVALID_STATE);
				return;
			}

			NETP_ASSERT(outp != nullptr && outp->len());
			m_outlets.push_back(std::move(outp));
			m_outlets_nbytes += outp->len();

			if (m_outlets_nbytes > m_bufsize) {
				m_flag |= f_buf_full;
				event_broker_any::invoke<fn_repeater_event_t>(repeater_event::e_buffer_full);
			}
			_do_write();
		}

		void finish() {
			if (!m_loop->in_event_loop()) {
				m_loop->schedule([rep = NRP<_self_t>(this)]() {
					rep->finish();
				});
				return;
			}

			if (m_flag & (f_finished | f_finish_pending)) {
				return;
			}

			//all write error would be triggered during write operation, so we only need to mark state here
			if ((m_outlets.size() == 0)) {
				NETP_ASSERT((m_flag&f_writing) ==0);
				m_flag |= f_finished;
				event_broker_any::invoke<fn_repeater_event_t>(repeater_event::e_finished);
			} else {
				m_flag |= f_finish_pending;
			}
		}
	};
}}

#endif