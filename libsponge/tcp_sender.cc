#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint16_t cur_window_size = _last_window_size ? _last_window_size : 1;
    while (cur_window_size > _bytes_in_flight) {
        TCPSegment seg;
        // 如果此时尚未发送 SYN 数据包，则立即发送
        if (!_syn_sent) {
            seg.header().syn = true;
            _syn_sent = true;
        }
        seg.header().seqno = next_seqno();
        const size_t payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, cur_window_size - _bytes_in_flight - seg.header().syn);
        string payload = _stream.read(payload_size);
        /**
         * 读取好后，如果满足以下条件，则增加 FIN
         *  1. 从来没发送过 FIN
         *  2. 输入字节流处于 EOF
         *  3. window 减去 payload 大小后，仍然可以存放下 FIN
         */
        if (!_fin_sent && _stream.eof() && payload.size() + _bytes_in_flight < cur_window_size) {
            seg.header().fin = true;
            _fin_sent = true;
        }
        seg.payload() = Buffer(move(payload));
        // 如果没有任何数据，则停止数据包的发送!!!!!
        if (seg.length_in_sequence_space() == 0)
            break;
        // 如果没有正在等待的数据包，则重设更新时间
        if (_outgoing.empty()) {
            _timeout = _initial_retransmission_timeout;
            _timecount = 0;
        }
        // 发送
        _segments_out.push(seg);

        _bytes_in_flight += seg.length_in_sequence_space();
        _outgoing.push(seg);
        _next_seqno += seg.length_in_sequence_space();

        if (seg.header().fin)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_seqno = unwrap(ackno, _isn, _next_seqno);
    if (abs_seqno > _next_seqno)
        return;
    // 将已经接收到的数据包丢弃
    while (!_outgoing.empty()) {
        TCPSegment seg = _outgoing.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= abs_seqno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _outgoing.pop();

            _timeout = _initial_retransmission_timeout;
            _timecount = 0;
            _consecutive_retransmissions_count = 0;
        } else
            break;
    }
    _last_window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timecount += ms_since_last_tick;
    if (!_outgoing.empty() && _timecount >= _timeout) {
        TCPSegment seg = _outgoing.front();
        if (_last_window_size)
            _timeout <<= 1;

        _timecount = 0;
        _segments_out.push(seg);
        _consecutive_retransmissions_count++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
