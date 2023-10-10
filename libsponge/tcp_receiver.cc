#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;
// LISTEN：等待 SYN 包的到来。若在 SYN 包到来前就有其他数据到来，则必须丢弃。
// SYN_RECV：获取到了 SYN 包，此时可以正常的接收数据包
// FIN_RECV：获取到了 FIN 包，此时务必终止 ByteStream 数据流的输入。
void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();
    if (!_set_syn_flag) {
        if (!header.syn)
            return;
        else {
            _isn = header.seqno;
            _set_syn_flag = true;
        }
    }
    uint64_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;
    uint64_t cur_ackno = unwrap(header.seqno, _isn, abs_ackno);
    _reassembler.push_substring(seg.payload().copy(), cur_ackno - 1 + (header.syn), header.fin);
}
// 由于 ISN 指代的是 SYN 的 SN，所以除了第一个带有 SYN 的 TCPsegment 以外，在我们计算其他的 TCPSegment 的 payload 时，需要减一
optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_set_syn_flag) {
        return {};
    }
    uint64_t x = _reassembler.stream_out().bytes_written() + 1;
    if (_reassembler.stream_out().input_ended()) {
        x += 1;
    }
    return wrap(x, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
