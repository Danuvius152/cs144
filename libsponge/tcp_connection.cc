#include "tcp_connection.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::_set_rst_state(bool send_rst) {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _is_active = false;
    if (send_rst) {
        TCPSegment seg;
        seg.header().rst = true;
        _segments_out.push(seg);
    }
}

void TCPConnection::_trans_segments_to_out_with_ack_and_win() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        if (_receiver.ackno().has_value()) {
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
            seg.header().ack = true;
        }
        _sender.segments_out().pop();
        _segments_out.push(seg);
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    // 如果发来的是一个 ACK 包，则无需发送 ACK
    bool need_send_ack = seg.length_in_sequence_space();

    if (seg.header().rst) {
        _set_rst_state(false);
        return;
    }
    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        // 队列中已经有数据报文段了就不需要专门的空包回复 ACK
        if (!_sender.segments_out().empty())
            need_send_ack = false;
    }

    // 被动连接
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED)
        return connect();
    // 被动关闭
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        _linger_after_streams_finish = false;
    // LAST_ACK 状态转移到 CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _is_active = false;
        return;
    }

    if (need_send_ack && TCPState::state_summary(_receiver) != TCPReceiverStateSummary::LISTEN)
        _sender.send_empty_segment();

    // 将等待发送的数据包加上本地的 ackno 和 window size
    _trans_segments_to_out_with_ack_and_win();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    size_t write_size = _sender.stream_in().write(data);
    _sender.fill_window();
    _trans_segments_to_out_with_ack_and_win();
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _set_rst_state(true);
        return;
    }
    _trans_segments_to_out_with_ack_and_win();
    _time_since_last_segment_received += ms_since_last_tick;
    // 如果处于 TIME_WAIT 状态并且超时，则可以静默关闭连接
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish &&
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // 在输入流结束后，必须立即发送 FIN
    _sender.fill_window();
    _trans_segments_to_out_with_ack_and_win();
}

void TCPConnection::connect() {
    // 第一次调用 _sender.fill_window 将会发送一个 syn 数据包
    _sender.fill_window();
    _is_active = true;
    _trans_segments_to_out_with_ack_and_win();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _set_rst_state(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
